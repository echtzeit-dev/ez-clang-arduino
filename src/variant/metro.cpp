#include "ez/device.h"

#include "ez/assert.h"
#include "ez/protocol.h"
#include "ez/response.h"
#include "ez/support.h"

#include "Arduino.h"

void device_setupSendReceive() {
  // CDC serial channel @ 9600 baud
  Serial.begin(9600);

  // Wait for the REPL (or the test driver) to send the handshake sequence to
  // start the session
  waitForHandshake();
}

bool device_receiveBytes(char Buffer[], uint32_t Count) {
  uint32_t Received = 0;
  while (Received < Count) {
    Received += Serial.readBytes(Buffer + Received, Count - Received);
  }
  return true;
}

void device_sendBytes(const char *Buffer, size_t Size) {
  Serial.write(Buffer, Size);
}

void device_flushReceiveBuffer() {
  Serial.flush();
}

void device_notifyBoot() {
  // Blink LED slowly and leave it off
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 3; ++i) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
  }
}

void device_notifyReady() {
  digitalWrite(LED_BUILTIN, HIGH); // Turn on LED
}

void device_notifyTick() {
  // Blink LED quickly
  for (int i = 0; i < 1; ++i) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
  }
}

void device_notifyShutdown() {
  digitalWrite(LED_BUILTIN, LOW); // Turn off LED
  delay(500);
}
