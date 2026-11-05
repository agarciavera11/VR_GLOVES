#include "haptic_engine.h"

static const char* const FINGER_NAMES[FINGER_COUNT] = {"pulgar", "indice", "medio", "anular",
                                                       "menique"};

void HapticEngine::begin(Print* log) {
  presentMask_ = 0;
  for (uint8_t f = 0; f < FINGER_COUNT; f++) {
    drivers_[f].attach(wire_, mux_, FINGER_TO_MUX_CHANNEL[f], DRV2605L_ADDR);
    const bool ok = drivers_[f].begin(LRA_RATED_VRMS, LRA_MAX_VPEAK, LRA_FREQ_HZ);
    if (ok) presentMask_ |= (uint8_t)(1u << f);
    if (log) {
      log->print(F("[haptic] dedo "));
      log->print(f);
      log->print(F(" ("));
      log->print(FINGER_NAMES[f]);
      log->print(F(") canal "));
      log->print(FINGER_TO_MUX_CHANNEL[f]);
      log->println(ok ? F(": DRV2605L OK") : F(": no detectado"));
    }
  }
  if (log && presentMask_ != 0) {
    log->print(F("[haptic] RATED_VOLTAGE=0x"));
    log->print(drivers_[__builtin_ctz(presentMask_)].ratedVoltageReg(), HEX);
    log->print(F(" OD_CLAMP=0x"));
    log->println(drivers_[__builtin_ctz(presentMask_)].odClampReg(), HEX);
  }
  if (LRA_AUTOCALIBRATE) calibrate(log);
}

void HapticEngine::calibrate(Print* log) {
  calibratedMask_ = 0;
  uint8_t pending = 0;
  // Lanza todas las calibraciones a la vez: cada DRV2605L trabaja solo.
  for (uint8_t f = 0; f < FINGER_COUNT; f++) {
    if ((presentMask_ & (1u << f)) && drivers_[f].startCalibration()) pending |= (uint8_t)(1u << f);
  }
  const uint32_t start = millis();
  while (pending != 0 && millis() - start < 2500) {
    for (uint8_t f = 0; f < FINGER_COUNT; f++) {
      if (!(pending & (1u << f))) continue;
      if (!drivers_[f].busy()) {
        pending &= (uint8_t)~(1u << f);
        if (drivers_[f].calibrationSucceeded()) calibratedMask_ |= (uint8_t)(1u << f);
      }
    }
    delay(20);
  }
  if (log) {
    log->print(F("[haptic] calibracion LRA: presentes=0b"));
    log->print(presentMask_, BIN);
    log->print(F(" calibrados=0b"));
    log->println(calibratedMask_, BIN);
    if (pending) log->println(F("[haptic] aviso: calibracion sin terminar (timeout)"));
  }
}

uint8_t HapticEngine::playEffect(uint8_t mask, uint8_t effectId) {
  uint8_t done = 0;
  for (uint8_t f = 0; f < FINGER_COUNT; f++) {
    if (!(mask & presentMask_ & (1u << f))) continue;
    pulseActive_[f] = false;
    if (drivers_[f].playEffect(effectId)) done |= (uint8_t)(1u << f);
  }
  return done;
}

uint8_t HapticEngine::playSequence(uint8_t mask, const uint8_t* ids, uint8_t count) {
  uint8_t done = 0;
  for (uint8_t f = 0; f < FINGER_COUNT; f++) {
    if (!(mask & presentMask_ & (1u << f))) continue;
    pulseActive_[f] = false;
    if (drivers_[f].playSequence(ids, count)) done |= (uint8_t)(1u << f);
  }
  return done;
}

uint8_t HapticEngine::pulse(uint8_t mask, uint8_t amplitude, uint16_t durationMs, uint32_t nowMs) {
  if (durationMs > MAX_PULSE_MS) durationMs = MAX_PULSE_MS;
  const uint8_t rtp = amplitude >> 1;  // 0..255 -> 0..127
  uint8_t done = 0;
  for (uint8_t f = 0; f < FINGER_COUNT; f++) {
    if (!(mask & presentMask_ & (1u << f))) continue;
    if (durationMs == 0 || rtp == 0) {
      drivers_[f].stop();
      pulseActive_[f] = false;
      continue;
    }
    if (drivers_[f].setRealtime(rtp)) {
      pulseActive_[f] = true;
      pulseEndMs_[f] = nowMs + durationMs;
      done |= (uint8_t)(1u << f);
    }
  }
  return done;
}

uint8_t HapticEngine::stop(uint8_t mask) {
  uint8_t done = 0;
  for (uint8_t f = 0; f < FINGER_COUNT; f++) {
    if (!(mask & presentMask_ & (1u << f))) continue;
    pulseActive_[f] = false;
    if (drivers_[f].stop()) done |= (uint8_t)(1u << f);
  }
  return done;
}

void HapticEngine::update(uint32_t nowMs) {
  for (uint8_t f = 0; f < FINGER_COUNT; f++) {
    // Resta con signo: funciona aunque millis() desborde (~49 días).
    if (pulseActive_[f] && (int32_t)(nowMs - pulseEndMs_[f]) >= 0) {
      drivers_[f].stop();
      pulseActive_[f] = false;
    }
  }
}
