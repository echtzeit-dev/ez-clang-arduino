#include "ez/symbols.h"

#include "ez/abi.h"
#include "ez/assert.h"
#include "ez/support.h"

#include <cstring>

// Reduced Elf32_Sym struct (relink output)
struct EzClang_Sym {
  uint32_t st_name;   // Symbol name (index into string table)
  uint32_t st_value;  // Value or address associated with the symbol
};

extern EzClang_Sym _ssymtab;
extern EzClang_Sym _esymtab;

extern const char _sstrtab;
extern const char _estrtab;

#define STRINGIFY(NAME) #NAME
#define X(NAME) { STRINGIFY(NAME), ptr2addr((void *)&NAME) }

static const Symbol BuiltinRPCEndpoints[] {
  X(__ez_clang_rpc_commit),
  X(__ez_clang_rpc_execute),
  X(__ez_clang_rpc_mem_read_cstring),
};

static const Symbol BuiltinRuntimeFunctions[] {
  X(__ez_clang_report_value),
  X(__ez_clang_report_string),
  X(__ez_clang_inline_heap_acquire)
};

uint32_t getBootstrapSymbols(const Symbol *BootstrapSyms[]) {
  static const Symbol BootstrapSym = X(__ez_clang_rpc_lookup);
  *BootstrapSyms = &BootstrapSym;
  return 1;
}

template <size_t Size>
uint32_t lookupUnordered(const Symbol (&Array)[Size], const char *Data,
                         uint32_t Length) {
  for (uint32_t i = 0; i < Size; i += 1)
    if (memcmp(Array[i].Name, Data, Length) == 0)
      return Array[i].Addr;
  return 0;
}

static constexpr uint32_t SymbolNotFound = 0;

uint32_t lookupBuiltinSymbol(const char *Data, uint32_t Length) {
  static constexpr const char *PrefixRPC = "__ez_clang_rpc_";
  if (memcmp(PrefixRPC, Data, strlen(PrefixRPC)) == 0) {
    if (uint32_t Addr = lookupUnordered(BuiltinRPCEndpoints, Data, Length))
      return Addr;
    fail("Builtin RPC endpoint not found");
  }
  static constexpr const char *PrefixRuntime = "__ez_clang_";
  if (memcmp(PrefixRuntime, Data, strlen(PrefixRuntime)) == 0) {
    if (uint32_t Addr = lookupUnordered(BuiltinRuntimeFunctions, Data, Length))
      return Addr;
    fail("Builtin runtime function not found");
  }
  return SymbolNotFound;
}

uint32_t lookupSymbol(const char *Data, uint32_t Length) {
  const EzClang_Sym *First = &_ssymtab;
  const EzClang_Sym *Last = &_esymtab - 1;
  while (First <= Last) {
    const EzClang_Sym *It = First + ((Last - First) / 2);
    const char *Str = &_sstrtab + It->st_name;
    int Cmp = memcmp(Data, Str, Length);
    if (Cmp > 0) {
      First = It + 1;
    } else if (Cmp < 0) {
      Last = It - 1;
    } else {
      return It->st_value;
    }
  }
  return 0;
}
