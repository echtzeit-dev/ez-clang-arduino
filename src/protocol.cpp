#include "ez/protocol.h"

#include "ez/assert.h"
#include "ez/response.h"
#include "ez/serialize.h"
#include "ez/support.h"
#include "ez/device.h"

#include <cstring>

constexpr uint32_t MessageHeaderSize = 32;

void sendMessage(EPCOpCode OpC, uint32_t SeqNo, const char Payload[],
                 uint32_t PayloadSize) {
  char HeaderBuffer[MessageHeaderSize];
  char *Data = HeaderBuffer;
  Data += writeUInt64(Data, PayloadSize + MessageHeaderSize);
  Data += writeUInt64(Data, OpC);
  Data += writeUInt64(Data, SeqNo);
  Data += writeUInt64(Data, 0);
  sendBytes(HeaderBuffer, MessageHeaderSize);
  sendBytes(Payload, PayloadSize);
}

void sendSetupMessage(char Buffer[], SetupInfo Info) {
  // Magic sequence is symmetric, we can ignore endianness
  // 00000001 00100011 01010111 10111101 10111101 01010111 00100011 00000001
  static const uint64_t SetupMagic = 0x012357BDBD572301ull;
  sendBytes((const char*)&SetupMagic, sizeof(uint64_t));

  char *Data = Buffer;
  Data += writeString(Data, Info.Version);
  Data += writeUInt64(Data, ptr2addr(Info.CodeBuffer));
  Data += writeUInt64(Data, Info.CodeBufferSize);
  Data += writeUInt64(Data, Info.NumSymbols);

  for (size_t i = 0; i < Info.NumSymbols; i++) {
    Data += writeString(Data, Info.Symbols[i].Name);
    Data += writeUInt64(Data, Info.Symbols[i].Addr);
  }

  sendMessage(Setup, 0, Buffer, Data - Buffer);
}

void sendHangupMessage(uint8_t ErrCode) {
  if (ErrCode == 0) {
    char SuccessByte = 0;
    sendMessage(Hangup, 0, &SuccessByte, sizeof(SuccessByte));
  } else {
    uint32_t Size;
    const char *Buffer = errorGetBuffer(Size);
    sendMessage(Hangup, 0, Buffer, Size);
  }
}

bool receiveMessage(char Buffer[], uint32_t BufferSize, HeaderInfo &Msg) {
  if (!receiveBytes(Buffer, MessageHeaderSize))
    fail("Error receiving message header. Shutting down.");

  // Parse all header fields, so we can reuse the buffer for the payload
  const char *Data = Buffer;
  Data += readSize(Data, Msg.PayloadBytes);
  Msg.PayloadBytes -= MessageHeaderSize;
  Data += readUInt64as32(Data, Msg.OpCode);
  Data += readUInt64as32(Data, Msg.SeqID);
  uint32_t TagAddr;
  Data += readAddr(Data, TagAddr);

  if (Msg.PayloadBytes > BufferSize) {
    errorEx(Buffer, BufferSize,
            "Message payload (%lu bytes) exceeds buffer size (%lu bytes)",
            Msg.PayloadBytes, BufferSize);
    flushReceiveBuffer();
    return false;
  }

  if (Msg.OpCode == Call)
    Msg.Handler = reinterpret_cast<RPCEndpoint *>(addr2ptr(TagAddr));

  if (Msg.OpCode != Call && Msg.OpCode != Hangup) {
    receiveBytes(Buffer, Msg.PayloadBytes);
    errorEx(Buffer, BufferSize, "Received unexpected message op-code: %lu",
            Msg.OpCode);
    return false;
  }

  return receiveBytes(Buffer, Msg.PayloadBytes);
}
