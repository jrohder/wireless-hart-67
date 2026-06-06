#include "HartBridge.h"

HartBridge::HartBridge()
    : uart(&Serial2),
      carrierDetected(false),
      lastUpdateTime(0),
      txBytes(0),
      rxBytes(0),
      txPackets(0),
      rxPackets(0),
      lastActivityMs(0),
      lastTxByteMs(0),
      lastRxByteMs(0) {}

void HartBridge::begin() {
  pinMode(HART_RTS_PIN, OUTPUT);
  digitalWrite(HART_RTS_PIN, LOW);
  pinMode(HART_OCD_PIN, INPUT);

  uart->begin(HART_UART_BAUD, SERIAL_8O1, HART_RX_PIN, HART_TX_PIN);
}

void HartBridge::update() {
  unsigned long now = millis();
  if (now - lastUpdateTime < 50) {
    return;
  }
  lastUpdateTime = now;
  carrierDetected = (digitalRead(HART_OCD_PIN) == HIGH);
}

void HartBridge::write(uint8_t byte) {
  uart->write(byte);
  noteTxByte();
}

int HartBridge::available() {
  return uart->available();
}

uint8_t HartBridge::read() {
  uint8_t byte = static_cast<uint8_t>(uart->read());
  noteRxByte();
  return byte;
}

bool HartBridge::isCarrierDetected() const { return carrierDetected; }

bool HartBridge::hadActivitySince(unsigned long sinceMs) const {
  return lastActivityMs >= sinceMs;
}

void HartBridge::noteTxByte() {
  unsigned long now = millis();
  if (txBytes == 0 || (now - lastTxByteMs) > kPacketIdleMs) {
    txPackets++;
  }
  txBytes++;
  lastTxByteMs = now;
  lastActivityMs = now;
}

void HartBridge::noteRxByte() {
  unsigned long now = millis();
  if (rxBytes == 0 || (now - lastRxByteMs) > kPacketIdleMs) {
    rxPackets++;
  }
  rxBytes++;
  lastRxByteMs = now;
  lastActivityMs = now;
}
