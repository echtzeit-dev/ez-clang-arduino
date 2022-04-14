#define __STDC_WANT_LIB_EXT2__ 1  // We want dynamic allocations in vasprintf

#include <algorithm>
#include <cinttypes>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <queue>
#include <regex>
#include <string>
#include <sstream>
#include <vector>

// Symbol table entries for ELF32 (input)
struct Elf32_Sym {
  uint32_t st_name;   // Symbol name (index into string table)
  uint32_t st_value;  // Value or address associated with the symbol
  uint32_t st_size;   // Size of the symbol
  uint8_t st_info;    // Symbol's type and binding attributes
  uint8_t st_other;   // Must be zero; reserved
  uint16_t st_shndx;  // Which section (header table index) it's defined in
};

// Reduced entries for ez-clang lookup (output)
struct EzClang_Sym {
  uint32_t st_name;   // Symbol name (index into string table)
  uint32_t st_value;  // Value or address associated with the symbol
};

void exitError(std::string message) {
  fprintf(stderr, "Error: %s\n", message.c_str());
  exit(1);
}

void warning(std::string message) {
  fprintf(stderr, "Warning: %s\n", message.c_str());
}

void printUsage(const char *argv0) {
  fprintf(stderr, "Post-process static symbol table data\n");
  fprintf(stderr, "Usage: %s [-v] [-q] [--log <logfile>] strtab.section symtab.section whitelist1.txt ...\n", argv0);
}

std::string int2hex(uint32_t val, size_t width) {
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(width) << std::hex << val;
  return ss.str();
}

bool isRegularFile(std::string filename) {
  using namespace std::filesystem;
  if (!exists(filename))
    exitError("Input file '" + std::string(filename) + "' not found");
  if (!is_regular_file(filename) &&
      !is_symlink(filename))
    exitError("Input file '" + std::string(filename) + "' is not a regular file or symlink");
  return true;
}

template <typename Fn>
std::queue<std::filesystem::path> parseArguments(int argc, char *argv[], Fn handleOpt) {
  std::vector<std::string> args_in(argv + 1, argv + argc);
  std::queue<std::filesystem::path> pos_args;
  for (int i = 0; i < args_in.size(); i += 1) {
    // Flags can be anywhere
    if (size_t consume = handleOpt(args_in, i)) {
      i += consume - 1;
      continue;
    }
    // All positional arguments are supposed to be files on disk
    if (isRegularFile(args_in[i]))
      pos_args.emplace(args_in[i]);
  }
  return pos_args;
}

std::vector<std::byte> loadFile(std::filesystem::path filepath) {
  std::ifstream ifs(filepath, std::ios::binary | std::ios::ate);
  if(!ifs)
    exitError(filepath.string() + " (" + std::strerror(errno) + ")");

  auto end = ifs.tellg();
  ifs.seekg(0, std::ios::beg);

  auto size = std::size_t(end - ifs.tellg());
  if(size == 0)
    exitError(filepath.string() + " (file size unknown)");

  std::vector<std::byte> buffer(size);
  if(!ifs.read((char*)buffer.data(), buffer.size()))
    exitError(filepath.string() + " (" + std::strerror(errno) + ")");

  return buffer;
}

std::vector<std::string> loadTextFile(std::filesystem::path filepath) {
  std::ifstream file(filepath);
  if (!file.is_open())
    exitError(filepath.string() + " (" + std::strerror(errno) + ")");

  std::string line;
  std::vector<std::string> lines;
  while (std::getline(file, line))
    lines.push_back(std::move(line));

  file.close();
  return lines;
}

std::vector<std::string> convertExportsFile(std::vector<std::string> exportsFile) {
  const std::regex symbolName("[_.$a-zA-Z][_.$a-zA-Z0-9]*");
  std::vector<std::string> exports;
  for (const std::string &line : exportsFile) {
    if (std::regex_match(line, symbolName))
      exports.push_back(std::move(line));
    else
      warning("Invalid symbol name '" + line + "' in exports file");
  }
  return exports;
}

size_t appendWhitelist(std::vector<std::string> &out, std::filesystem::path filepath) {
  std::vector<std::string> fileList = loadTextFile(filepath);
  out.reserve(out.size() + fileList.size());

  size_t count = 0;
  const std::regex symbolName("[_.$a-zA-Z][_.$a-zA-Z0-9]*");
  for (std::string &line : fileList) {
    if (line.empty())
      continue;
    if (!std::regex_match(line, symbolName))
      warning("Unconventional symbol name '" + line + "' in exports file '" +
              filepath.string() + "'");
    count += 1;
    out.emplace_back(std::move(line));
  }
  return count;
}

std::vector<const Elf32_Sym *> convertSymtab(const std::vector<std::byte> &symtab,
                                             const std::vector<std::byte> &strtab,
                                             bool debugDump) {
  if (symtab.size() % sizeof(Elf32_Sym) != 0)
    exitError("symtab invalid or contains padding");

  size_t numSymbols = symtab.size() / sizeof(Elf32_Sym);
  std::vector<const Elf32_Sym *> symbols;
  symbols.reserve(numSymbols);

  const Elf32_Sym *begin = reinterpret_cast<const Elf32_Sym *>(symtab.data());
  const Elf32_Sym *end = begin + numSymbols;
  const char *strtabBase = reinterpret_cast<const char *>(strtab.data());
  for (const Elf32_Sym *it = begin; it < end; it += 1) {
    uint32_t offset = it->st_name;
    if (offset >= strtab.size())
      exitError("Invalid strtab offset in symtab entry " +
                std::to_string(it - begin) + " (out of range)");
    if (debugDump)
      if (offset != 0 && strtabBase[offset - 1] != '\0') {
        uint32_t str = offset;
        while (str > 0 && strtabBase[str - 1] != '\0') str -= 1;
        warning("strtab offset in symtab entry " + std::to_string(it - begin) +
                " points into the middle of a name:\n" +
                std::string(strtabBase + str) + '\n' +
                std::string(offset - str, ' ') + '^');
      }
    const char *name = strtabBase + offset;
    if (offset + strlen(name) >= strtab.size())
      exitError("Unterminated string in symtab entry " + std::to_string(it - begin));
    symbols.push_back(it);
  }

  return symbols;
}

size_t checkStrtab(const std::vector<std::byte> &strtab) {
  std::vector<std::string> strings;
  const std::byte *it = strtab.data();
  const std::byte *str = it;
  const std::byte *end = it + strtab.size();
  while (it < end) {
    const char ch = static_cast<char>(*it);
    if (static_cast<char>(*it) == '\0') {
      strings.emplace_back(reinterpret_cast<const char *>(str), it - str);
      str = it + 1;
    }
    it += 1;
  }

  // Validate that last character was null-terminator
  if (str != end)
    exitError("strtab contains unterminated string");

  // Sort and echo duplicates (valid symtabs should not have dupes)
  std::sort(strings.begin(), strings.end());
  auto dupe = strings.begin();
  while ((dupe = std::adjacent_find(dupe, strings.end())) != strings.end())
    warning("Duplicate entry in strtab '" + *dupe + "'");

  return strings.size();
}

void filterSymbols(std::vector<const Elf32_Sym *> &symbols,
                   const std::vector<std::byte> &strtab,
                   const std::vector<std::string> &exports, bool debugDump) {
  if (debugDump)
    printf("\nDropping symbols:\n");

  const char *strtabBase = reinterpret_cast<const char *>(strtab.data());
  auto notExported = [&](const Elf32_Sym *&symbol) {
    if (symbol->st_name == 0)
      return true;
    std::string name = strtabBase + symbol->st_name;
    auto it = std::find(exports.begin(), exports.end(), name);
    if (debugDump && it == exports.end())
      printf("'%s' ", name.c_str());
    return it == exports.end();
  };

  auto eraseFrom = std::remove_if(symbols.begin(), symbols.end(), notExported);
  symbols.erase(eraseFrom, symbols.end());

  if (debugDump)
    printf("\n\n");
}

void filterStrings(std::vector<std::string> &strings,
                   const std::vector<std::string> &exports, bool debugDump) {
  if (debugDump)
    printf("\nDropping strings:\n");

  auto notExported = [&](const std::string &name) {
    auto it = std::lower_bound(exports.begin(), exports.end(), name);
    if (debugDump && *it != name)
      printf("'%s' ", name.c_str());
    return *it != name;
  };

  auto eraseFrom = std::remove_if(strings.begin(), strings.end(), notExported);
  strings.erase(eraseFrom, strings.end());

  if (debugDump)
    printf("\n\n");
}

void sortSymbols(std::vector<const Elf32_Sym *> &symbols,
                 const std::vector<std::byte> &strtab) {
  const char *strtabBase = reinterpret_cast<const char *>(strtab.data());
  std::sort(symbols.begin(), symbols.end(),
            [strtabBase](const Elf32_Sym *a, const Elf32_Sym *b) {
              const char *name_a = strtabBase + a->st_name;
              const char *name_b = strtabBase + b->st_name;
              return std::strcmp(name_a, name_b) < 0;
            });
}

std::pair<std::unique_ptr<EzClang_Sym[]>, std::string>
reencode(const std::vector<const Elf32_Sym *> &symbols,
         const std::vector<std::byte> &strtabIn,
         const std::filesystem::path logfile, bool debugDump) {
  std::ostringstream strtabOS;
  strtabOS << '\0'; // By convention, strtab starts with a null character
  const char *strtabBase = reinterpret_cast<const char *>(strtabIn.data());

  if (debugDump)
    printf("\nWriting ez-clang symtab:\n");

  std::ofstream logOS;
  std::function<void(std::string, uint32_t)> log = nullptr;
  if (!logfile.empty()) {
    logOS = std::ofstream(logfile);
    log = [&](const std::string &name, uint32_t addr) {
      logOS << int2hex(addr, 8) << "   " << name << "\n";
    };
  }

  std::unique_ptr<EzClang_Sym[]> symtabOut(new EzClang_Sym[symbols.size()]);
  for (size_t i = 0; i < symbols.size(); i += 1) {
    const Elf32_Sym &sym = *symbols[i];
    std::string name = strtabBase + sym.st_name;
    uint32_t offset = strtabOS.tellp();
    strtabOS << name << '\0';
    symtabOut[i] = EzClang_Sym{offset,sym.st_value};
    if (debugDump)
      printf("  0x%08" PRIx32 " %s\n", sym.st_value, name.c_str());
    if (!logfile.empty())
      log(name, sym.st_value);
  }

  if (debugDump)
    printf("\n");
  if (!logfile.empty())
    logOS.close();
  return std::make_pair(std::move(symtabOut), strtabOS.str());
}

bool g_quiet = false;
bool g_verbose = false;
std::filesystem::path g_logfile;

int println(const char *__restrict fmt, ...) {
  if (g_quiet)
    return 0;
  va_list args;
  va_start(args, fmt);
  char *outstr = nullptr;
  int len = vasprintf(&outstr, fmt, args);
  va_end(args);
  if (len < 0)
    exitError("Cannot print '" + std::string(fmt) + "' (" + std::strerror(errno) + ")");
  puts(outstr);
  free(outstr);
  return len;
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    printUsage(argv[0]);
    return 1;
  }

  auto handleOption = [&](const std::vector<std::string> &args, int idx) {
    const std::string &arg = args[idx];
    if (arg == "-v" || arg == "--verbose") {
      g_verbose = true;
      return 1;
    }
    if (arg == "-q" || arg == "--quiet") {
      g_quiet = true;
      return 1;
    }
    if (arg == "-log" || arg == "--log") {
      if (args.size() > idx + 1)
        g_logfile = args[idx + 1];
      return 2;
    }
    return 0;
  };

  std::queue<std::filesystem::path> args = parseArguments(argc, argv, handleOption);
  std::filesystem::path strtabFile = std::move(args.front()); args.pop();
  std::filesystem::path symtabFile = std::move(args.front()); args.pop();

  println("Inputs:");

  const std::vector<std::byte> strtabIn = loadFile(strtabFile);
  size_t numStringsIn = checkStrtab(strtabIn);
  println("  strtab: %s, size: %lu, strings: %lu", strtabFile.c_str(), strtabIn.size(), numStringsIn);

  const std::vector<std::byte> symtabIn = loadFile(symtabFile);
  std::vector<const Elf32_Sym *> symbols = convertSymtab(symtabIn, strtabIn, g_verbose && !g_quiet);
  size_t numSymbolsIn = symbols.size();
  println("  symtab: %s, size: %lu, symbols: %lu", symtabFile.c_str(), symtabIn.size(), numSymbolsIn);

  println("Whitelists:");
  std::vector<std::string> exports;
  while (!args.empty()) {
    size_t items = appendWhitelist(exports, args.front());
    println("  %s (%lu)", args.front().c_str(), items);
    args.pop();
  }

  filterSymbols(symbols, strtabIn, exports, g_verbose && !g_quiet);
  sortSymbols(symbols, strtabIn);

  println("Processing:");
  size_t numSymbolsOut = symbols.size();
  size_t symtabSize = sizeof(EzClang_Sym) * numSymbolsOut;
  println("  dropping %lu symbols", numSymbolsIn - numSymbolsOut);

  std::string strtabOut;
  std::unique_ptr<EzClang_Sym[]> symtabOut;
  std::tie(symtabOut, strtabOut) = reencode(symbols, strtabIn, g_logfile, g_verbose && !g_quiet);
  float symtabRatio = (100.f * symtabSize) / symtabIn.size() - 100.f;
  float strtabRatio = (100.f * strtabOut.size()) / strtabIn.size() - 100.f;
  println("  memory footprint: symtab %.1f%%, strtab %.1f%%", symtabRatio, strtabRatio);

  println("Outputs:");
  strtabFile.replace_extension(".exports");
  symtabFile.replace_extension(".exports");
  size_t numStringsOut = std::count(strtabOut.begin(), strtabOut.end(), '\0');
  println("  strtab: %s, size: %lu, strings: %lu", strtabFile.c_str(), strtabOut.size(), numStringsOut);
  println("  symtab: %s, size: %lu, symbols: %lu", symtabFile.c_str(), symtabSize, numSymbolsOut);

  std::ofstream strtabOS(strtabFile, std::ios::binary);
  strtabOS.write(reinterpret_cast<char*>(strtabOut.data()), strtabOut.size());
  strtabOS.close();

  std::ofstream symtabOS(symtabFile, std::ios::binary);
  symtabOS.write(reinterpret_cast<char*>(symtabOut.get()), symtabSize);
  symtabOS.close();

  println("Done");
  return 0;
}
