// drv2605l.h — Driver mínimo del DRV2605L (TI) para LRA, detrás de un TCA9548A.
//
// Se escribe directo sobre Wire (sin la librería de Adafruit) para controlar
// exactamente cuántas transacciones I2C hace cada comando: eso es lo que manda
// en la latencia. Registros según la hoja de datos TI SLOS854.
#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "tca9548a.h"

namespace drv {
// Registros
constexpr uint8_t REG_STATUS = 0x00;
constexpr uint8_t REG_MODE = 0x01;
constexpr uint8_t REG_RTP_INPUT = 0x02;
constexpr uint8_t REG_LIBRARY = 0x03;
constexpr uint8_t REG_WAVESEQ1 = 0x04;  // ..0x0B
constexpr uint8_t REG_GO = 0x0C;
constexpr uint8_t REG_RATED_VOLTAGE = 0x16;
constexpr uint8_t REG_OD_CLAMP = 0x17;
constexpr uint8_t REG_A_CAL_COMP = 0x18;
constexpr uint8_t REG_A_CAL_BEMF = 0x19;
constexpr uint8_t REG_FEEDBACK = 0x1A;
constexpr uint8_t REG_CONTROL1 = 0x1B;
constexpr uint8_t REG_CONTROL2 = 0x1C;
constexpr uint8_t REG_CONTROL3 = 0x1D;
constexpr uint8_t REG_CONTROL4 = 0x1E;

// Modos (MODE[2:0])
constexpr uint8_t MODE_INTTRIG = 0x00;
constexpr uint8_t MODE_RTP = 0x05;
constexpr uint8_t MODE_AUTOCAL = 0x07;
constexpr uint8_t MODE_STANDBY_BIT = 0x40;

constexpr uint8_t LIBRARY_LRA = 6;
}  // namespace drv

class Drv2605l {
 public:
  Drv2605l() = default;
  void attach(TwoWire& wire, Tca9548a& mux, uint8_t channel, uint8_t addr);

  // Comprueba que hay un DRV2605/DRV2605L en el canal y lo configura para LRA
  // en lazo cerrado. No calibra (ver startCalibration).
  bool begin(float ratedVrms, float maxVpeak, float freqHz);

  // Autocalibración no bloqueante: se puede lanzar en todos los dedos a la vez.
  bool startCalibration();
  bool busy();                 // true mientras GO = 1
  bool calibrationSucceeded();  // lee DIAG_RESULT tras terminar

  // Reproducción
  bool playEffect(uint8_t effectId);                        // 1 efecto de librería
  bool playSequence(const uint8_t* ids, uint8_t count);     // hasta 8 efectos
  bool setRealtime(uint8_t amplitude);                      // 0..127, entra en RTP
  bool stop();

  bool present() const { return present_; }
  uint8_t channel() const { return channel_; }

  // Valores de registro calculados (útiles para depurar por serie)
  uint8_t ratedVoltageReg() const { return ratedReg_; }
  uint8_t odClampReg() const { return odReg_; }

 private:
  bool select();
  bool writeReg(uint8_t reg, uint8_t value);
  bool writeRegs(uint8_t reg, const uint8_t* values, uint8_t count);
  bool readReg(uint8_t reg, uint8_t& value);
  bool setMode(uint8_t mode);

  TwoWire* wire_ = nullptr;
  Tca9548a* mux_ = nullptr;
  uint8_t channel_ = 0;
  uint8_t addr_ = 0x5A;
  bool present_ = false;
  uint8_t mode_ = 0xFF;  // caché del modo actual para no reescribirlo
  uint8_t ratedReg_ = 0;
  uint8_t odReg_ = 0;
};
