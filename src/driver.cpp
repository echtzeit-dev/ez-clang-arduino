#include "ez/assert.h"
#include "ez/device.h"
#include "ez/response.h"
#include "ez/protocol.h"
#include "ez/serialize.h"
#include "ez/support.h"
#include "ez/symbols.h"

#include <csetjmp>
#include <cstddef>
#include <cstdint>

#define EZ_CLANG_PROTOCOL_VERSION_STR "0.0.5"

//
// Static buffer for RPC requests and responses
//
char MessageBuffer[0x400] __attribute__((aligned(0x100)));

//
// Boundaries of the code buffer are provided from linker script
//
extern char _scode_buffer;
extern char _ecode_buffer;

void ez_clang_setup() {
  device_setupSendReceive();

  SetupInfo Info;
  Info.Version = EZ_CLANG_PROTOCOL_VERSION_STR;
  Info.CodeBuffer = &_scode_buffer;
  Info.CodeBufferSize = &_ecode_buffer - &_scode_buffer;
  Info.NumSymbols = getBootstrapSymbols(&Info.Symbols);
  sendSetupMessage(MessageBuffer, Info);
}

bool ez_clang_tick(uint8_t &ErrCode) {
  // Reserve the entire MessageBuffer for input.
  responseClearBuffer();

  // Explicit errors during message processing can be recoverable. We send
  // back an error response and wait for the next message.
  HeaderInfo Msg;
  if (!receiveMessage(MessageBuffer, c_array_size(MessageBuffer), Msg)) {
    uint32_t Size;
    const char *ErrResp = errorGetBuffer(Size);
    sendMessage(Result, Msg.SeqID, ErrResp, Size);
    return true;
  }

  // Shutdown and confirm with Hangup message
  if (Msg.OpCode == Hangup) {
    ErrCode = 0;
    return false;
  }

  // Define the ResponseBuffer in direct succession to the input message.
  char *InputEnd = MessageBuffer + Msg.PayloadBytes;
  size_t RemainingCapacity = c_array_size(MessageBuffer) - Msg.PayloadBytes;
  const char *RespBegin = responseSetBuffer(InputEnd, RemainingCapacity);

  // Invoke the handler for the requested endpoint. Handlers can use the error()
  // function to write error responses.
  const char *RespEnd = Msg.Handler(MessageBuffer, Msg.PayloadBytes);

  // Send the response back to the host and finish this tick.
  sendMessage(Result, Msg.SeqID, RespBegin, RespEnd - RespBegin);
  return true;
}

//
// Arduino initialization function
//
extern "C" void setup() {
  device_notifyBoot();
}

//
// Arduino main entrypoint
//
extern "C" void loop() {
  GlobalAssertionFailureBuffer = MessageBuffer;
  GlobalAssertionFailureBufferSize = c_array_size(MessageBuffer);

  // If an assertion fails, we longjmp back here and get a non-zero error code
  int EC = setjmp(GlobalAssertionFailureReturnPoint);
  uint8_t ErrCode = EC; // EC >= 0 && EC <= 0x7f

  // If we didn't get here by longjmp, then this is a restart
  if (ErrCode == 0) {
    device_notifyReady();
    ez_clang_setup();
    while (ez_clang_tick(ErrCode))
      device_notifyTick();
  }

  sendHangupMessage(ErrCode);
  device_notifyShutdown();
}
