#ifndef EZ_SERIAL_H
#define EZ_SERIAL_H

#include <cstddef>
#include <cstdint>

void notifyReady();
void notifyShutdown();

void setupSendReceive();
void sendBytes(const char *Buffer, size_t Size);
bool receiveBytes(char Buffer[], uint32_t Count);

void flushReceiveBuffer();
void flushBytes(char Buffer[], uint32_t Count);

#endif // EZ_SERIAL_H
