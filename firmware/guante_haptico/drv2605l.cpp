#include "drv2605l.h"

#include <math.h>

using namespace drv;

void Drv2605l::attach(TwoWire& wire, Tca9548a& mux, uint8_t channel, uint8_t addr) {
  wire_ = &wire;
  mux_ = &mux;
  channel_ = channel;
  addr_ = addr;
}

bool Drv2605l::select() { return mux_ != nullptr && mux_->select(channel_); }

bool Drv2605l::writeReg(uint8_t reg, uint8_t value) {
  if (!select()) return false;
  wire_->beginTransmission(addr_);
  wire_->write(reg);
  wire_->write(value);
  return wire_->endTransmission() == 0;
}

// Escritura de varios registros consecutivos en una sola transacción
// (el DRV2605L autoincrementa la dirección).
bool Drv2605l::writeRegs(uint8_t reg, const uint8_t* values, uint8_t count) {
  if (!select()) return false;
  wire_->beginTransmission(addr_);
  wire_->write(reg);
  for (uint8_t i = 0; i < count; i++) wire_->write(values[i]);
  return wire_->endTransmission() == 0;
}

bool Drv2605l::readReg(uint8_t reg, uint8_t& value) {
  if (!select()) return false;
  wire_->beginTransmission(addr_);
  wire_->write(reg);
  if (wire_->endTransmission(false) != 0) return false;
  if (wire_->requestFrom(addr_, (uint8_t)1) != 1) return false;
  value = (uint8_t)wire_->read();
  return true;
}

bool Drv2605l::setMode(uint8_t mode) {
  if (mode == mode_) return true;
  if (!writeReg(REG_MODE, mode)) {
    mode_ = 0xFF;
    return false;
  }
  mode_ = mode;
  return true;
}

bool Drv2605l::begin(float ratedVrms, float maxVpeak, float freqHz) {
  present_ = false;
  mode_ = 0xFF;
  uint8_t status = 0;
  if (!readReg(REG_STATUS, status)) return false;
  const uint8_t deviceId = status >> 5;  // 3 = DRV2605, 7 = DRV2605L
  if (deviceId != 3 && deviceId != 7) return false;

  // Salir de standby, modo de disparo interno.
  if (!setMode(MODE_INTTRIG)) return false;

  // RATED_VOLTAGE para LRA en lazo cerrado (SLOS854, ec. de V(LRA-CL_RMS)):
  //   V = 20.71e-3 * RV / sqrt(1 - (4*tSample + 300e-6) * f),  tSample = 300 µs
  const float k = sqrtf(1.0f - (4.0f * 300e-6f + 300e-6f) * freqHz);
  float rv = ratedVrms * k / 20.71e-3f;
  // OD_CLAMP en lazo cerrado: Vpeak = 21.96e-3 * OD_CLAMP
  float od = maxVpeak / 21.96e-3f;
  ratedReg_ = (uint8_t)constrain((int)lroundf(rv), 0, 255);
  odReg_ = (uint8_t)constrain((int)lroundf(od), 0, 255);

  // DRIVE_TIME ≈ medio periodo del LRA: t(ms) = 0.5 + 0.1 * DRIVE_TIME
  const float halfPeriodMs = 500.0f / freqHz;
  const uint8_t driveTime = (uint8_t)constrain((int)lroundf((halfPeriodMs - 0.5f) / 0.1f), 0, 31);

  bool ok = true;
  ok &= writeReg(REG_RATED_VOLTAGE, ratedReg_);
  ok &= writeReg(REG_OD_CLAMP, odReg_);
  // FEEDBACK: N_ERM_LRA=1 (LRA), FB_BRAKE_FACTOR=3, LOOP_GAIN=1, BEMF_GAIN=2
  ok &= writeReg(REG_FEEDBACK, 0x80 | (3 << 4) | (1 << 2) | 2);
  // CONTROL1: STARTUP_BOOST=1 + DRIVE_TIME
  ok &= writeReg(REG_CONTROL1, 0x80 | driveTime);
  // CONTROL3: lazo cerrado LRA, RTP en formato con signo (0 = sin señal)
  ok &= writeReg(REG_CONTROL3, 0x80);
  ok &= writeReg(REG_LIBRARY, LIBRARY_LRA);
  ok &= writeReg(REG_RTP_INPUT, 0x00);
  present_ = ok;
  return ok;
}

bool Drv2605l::startCalibration() {
  if (!present_) return false;
  bool ok = true;
  // CONTROL4: AUTO_CAL_TIME = 3 (1000–1200 ms)
  ok &= writeReg(REG_CONTROL4, 3 << 4);
  ok &= setMode(MODE_AUTOCAL);
  ok &= writeReg(REG_GO, 0x01);
  return ok;
}

bool Drv2605l::busy() {
  uint8_t go = 0;
  if (!readReg(REG_GO, go)) return false;
  return (go & 0x01) != 0;
}

bool Drv2605l::calibrationSucceeded() {
  uint8_t status = 0;
  if (!readReg(REG_STATUS, status)) return false;
  const bool ok = (status & 0x08) == 0;  // DIAG_RESULT = 0 => éxito
  setMode(MODE_INTTRIG);
  return ok;
}

bool Drv2605l::playEffect(uint8_t effectId) {
  const uint8_t seq[2] = {effectId, 0};
  return playSequence(seq, 2);
}

bool Drv2605l::playSequence(const uint8_t* ids, uint8_t count) {
  if (!present_ || count == 0) return false;
  if (count > 8) count = 8;
  uint8_t slots[8] = {0};
  for (uint8_t i = 0; i < count; i++) slots[i] = ids[i];
  const uint8_t used = count < 8 ? count + 1 : 8;  // + terminador 0
  bool ok = setMode(MODE_INTTRIG);
  ok &= writeRegs(REG_WAVESEQ1, slots, used);
  ok &= writeReg(REG_GO, 0x01);
  return ok;
}

bool Drv2605l::setRealtime(uint8_t amplitude) {
  if (!present_) return false;
  if (amplitude > 127) amplitude = 127;
  // Primero el valor y luego el modo: así el primer ciclo ya sale con amplitud.
  bool ok = writeReg(REG_RTP_INPUT, amplitude);
  ok &= setMode(MODE_RTP);
  return ok;
}

bool Drv2605l::stop() {
  if (!present_) return false;
  if (mode_ == MODE_RTP) return writeReg(REG_RTP_INPUT, 0x00);
  return writeReg(REG_GO, 0x00);
}
