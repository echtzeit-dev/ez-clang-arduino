#ifndef EZ_DEVICE_H
#define EZ_DEVICE_H

#include <cstddef>
#include <cstdint>

void device_notifyBoot();
void device_notifyReady();
void device_notifyTick();
void device_notifyShutdown();

void device_setupSendReceive();
void device_sendBytes(const char *Buffer, size_t Size);
bool device_receiveBytes(char Buffer[], uint32_t Count);

void device_flushReceiveBuffer();

#endif // EZ_DEVICE_H
