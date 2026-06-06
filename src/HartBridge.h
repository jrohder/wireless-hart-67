#ifndef HART_BRIDGE_H
#define HART_BRIDGE_H

#include <Arduino.h>
#include "Config.h"

// Transparent UART bridge to the AD5700-1 HART modem.
// Future HART command handlers can hook read()/write() without restructuring.
class HartBridge {
public:
  HartBridge();
  void begin();
  void update();

  void write(uint8_t byte);
  int available();
  uint8_t read();

  bool isCarrierDetected() const;
  uint32_t getTxBytes() const { return txBytes; }
  uint32_t getRxBytes() const { return rxBytes; }
  uint32_t getTxPackets() const { return txPackets; }
  uint32_t getRxPackets() const { return rxPackets; }
  unsigned long getLastActivityMs() const { return lastActivityMs; }
  bool hadActivitySince(unsigned long sinceMs) const;

private:
  void noteTxByte();
  void noteRxByte();

  HardwareSerial *uart;
  bool carrierDetected;
  unsigned long lastUpdateTime;

  uint32_t txBytes;
  uint32_t rxBytes;
  uint32_t txPackets;
  uint32_t rxPackets;
  unsigned long lastActivityMs;
  unsigned long lastTxByteMs;
  unsigned long lastRxByteMs;

  static const unsigned long kPacketIdleMs = 100;
};

#endif  // HART_BRIDGE_H
