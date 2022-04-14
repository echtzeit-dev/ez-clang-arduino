#include "ez/device.h"

#include "ez/assert.h"
#include "ez/response.h"
#include "ez/support.h"

#include "Arduino.h"

void setupSendReceive() {
  // Configure serial stream @ 9600 baud via UART
  Serial.begin(9600, UARTClass::Mode_8N1);

  // We always receive data synchronously, no need for interrupts
  NVIC_DisableIRQ(UART_IRQn);

  // Discard any existing data (host will connect and send data only after
  // receiving the setup message)
  flushReceiveBuffer();
}

bool receiveBytes(char Buffer[], uint32_t Count) {
  // Read from in-bound FIFO directly
  for (uint32_t i = 0; i < Count; i += 1) {
    while ((UART->UART_SR & UART_SR_RXRDY) == 0);
    Buffer[i] = UART->UART_RHR;
  }
  return true;
}

void sendBytes(const char *Buffer, size_t Size) {
  // Write to out-bound FIFO directly
  for (uint32_t i = 0; i < Size; i += 1) {
    while ((UART->UART_SR & UART_SR_TXRDY) == 0);
    UART->UART_THR = Buffer[i];
  }

  // Wait for transmission to complete
  while ((UART->UART_SR & UART_SR_TXEMPTY) == 0);
}

void flushReceiveBuffer() {
  while ((UART->UART_SR & UART_SR_RXRDY) != 0)
    UART->UART_RHR;
}

void notifyReady() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // Turn on LED
}

void notifyShutdown() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW); // Turn off LED
}
