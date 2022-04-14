#include "ez-config.h"

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

//
// Static buffer for RPC requests and responses
//
char MessageBuffer[0x2000] __attribute__((aligned(0x100)));

//
// Boundaries of the code buffer are provided from linker script
//
extern char _scode_buffer;
extern char _ecode_buffer;

//
// Arduino initialization function
//
extern "C" void setup() {
  GlobalAssertionFailureBuffer = MessageBuffer;
  GlobalAssertionFailureBufferSize = c_array_size(MessageBuffer);
  setupSendReceive();
  notifyShutdown();
}

void restart() {
  SetupInfo Info;
  Info.Version = EZ_CLANG_ARDUINO_VERSION_STR;
  Info.CodeBuffer = &_scode_buffer;
  Info.CodeBufferSize = &_ecode_buffer - &_scode_buffer;
  Info.NumSymbols = getBootstrapSymbols(&Info.Symbols);
  sendSetupMessage(MessageBuffer, Info);
}

uint8_t runMessageLoop() {
  while (true) {
    // Reserve the entire MessageBuffer for input. Errors are fatal and cause a
    // hangup. Code that issues errors must use the errorEx() function.
    responseClearBuffer();

    // Explicit errors during message processing can be recoverable. We send
    // back an error response and wait for the next message.
    HeaderInfo Msg;
    if (!receiveMessage(MessageBuffer, c_array_size(MessageBuffer), Msg)) {
      uint32_t Size;
      const char *ErrResp = errorGetBuffer(Size);
      sendMessage(Result, Msg.SeqID, ErrResp, Size);
      continue;
    }

    // Shutdown and confirm with Hangup message
    if (Msg.OpCode == Hangup)
      return 0;

    // Invoke the requested endpoint. Handlers can use the error() function to
    // write error responses. We define the ResponseBuffer in direct succession
    // to the input message.
    const char *RespBegin =
        responseSetBuffer(MessageBuffer + Msg.PayloadBytes,
                          c_array_size(MessageBuffer) - Msg.PayloadBytes);
    const char *RespEnd = Msg.Handler(MessageBuffer, Msg.PayloadBytes);
    sendMessage(Result, Msg.SeqID, RespBegin, RespEnd - RespBegin);
  }
}

//
// Arduino main entrypoint
//
extern "C" void loop() {
  // We will longjmp back here if an assertion fails and get a non-zero result
  int EC = setjmp(GlobalAssertionFailureReturnPoint);

  //assert((EC & 0xff) == 0, "longjmp error code out of bounds");
  uint8_t ErrCode = EC;

  // If we didn't get here by longjmp, then this is a restart
  if (ErrCode == 0) {
    restart();
    notifyReady();
    ErrCode = runMessageLoop();
  }

  notifyShutdown();
  sendHangupMessage(ErrCode);
}
