#ifndef EZ_PROTOCOL_H
#define EZ_PROTOCOL_H

#include "ez/abi.h"
#include "ez/serialize.h"
#include "ez/symbols.h"

#include <cstddef>
#include <cstdint>

// Magic sequence is symmetric, we can ignore endianness
// 00000001 00100011 01010111 10111101 10111101 01010111 00100011 00000001
static const uint64_t SetupMagic = 0x012357BDBD572301ull;

typedef char* RPCEndpoint(const char *Data, size_t Size);

enum EPCOpCode : uint32_t {
  Setup,
  Hangup,
  Result,
  Call,
  ReportValue,
  ReportString,
  LastOpC = ReportString
};

struct SetupInfo {
  const char *Version;
  char *CodeBuffer;
  uint32_t CodeBufferSize;
  uint32_t NumSymbols;
  const Symbol *Symbols;
};

struct HeaderInfo {
  uint32_t SeqID;
  uint32_t OpCode;
  uint32_t PayloadBytes;
  RPCEndpoint *Handler;
};

bool receiveMessage(char Buffer[], uint32_t BufferSize, HeaderInfo &Msg);

void sendMessage(EPCOpCode OpC, uint32_t SeqID, const char Payload[],
                 uint32_t NumArgBytes);

void sendSetupMessage(char Buffer[], SetupInfo Info);

void sendHangupMessage(uint8_t ErrCode);


#endif // EZ_PROTOCOL_H
