#include "tca9548a.h"

bool Tca9548a::begin(TwoWire& wire) {
  wire_ = &wire;
  present_ = disableAll();
  return present_;
}

bool Tca9548a::write(uint8_t mask) {
  wire_->beginTransmission(addr_);
  wire_->write(mask);
  return wire_->endTransmission() == 0;
}

bool Tca9548a::select(uint8_t channel) {
  if (channel > 7) return false;
  if (channel == current_) return true;  // ahorra una transacción I2C
  if (!write((uint8_t)(1u << channel))) {
    current_ = 0xFF;
    return false;
  }
  current_ = channel;
  return true;
}

bool Tca9548a::disableAll() {
  const bool ok = write(0x00);
  current_ = ok ? 0xFE : 0xFF;  // 0xFE = ninguno seleccionado
  return ok;
}
