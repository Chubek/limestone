#pragma once
#include "metacode/metacode.hpp"
#include <map>

namespace limestone::bin2bin {
enum class ByteOrder { Little, Big };
enum class ElfClass { Elf32=1, Elf64=2 };
struct ObjectFormat {
  uint16_t machine=0;
  ByteOrder byte_order=ByteOrder::Little;
  uint32_t flags=0;
  uint8_t osabi=0, abi_version=0;
  ElfClass elf_class=ElfClass::Elf64;
  bool operator==(const ObjectFormat&) const = default;
};
inline constexpr uint32_t object_undefined=UINT32_MAX, object_absolute=UINT32_MAX-1;
struct ObjectSection {
  std::string name;
  uint32_t type=1; // ELF PROGBITS (1), NOTE (7) or NOBITS (8).
  uint64_t flags=0, alignment=1, entry_size=0;
  std::vector<uint8_t> bytes;
  uint64_t zero_fill=0;
  uint64_t size() const { return type==8?zero_fill:bytes.size(); }
};
enum class SymbolBinding { Local, Global, Weak };
struct ObjectSymbol {
  std::string name;
  uint32_t section=object_undefined;
  uint64_t value=0, size=0;
  SymbolBinding binding=SymbolBinding::Global;
  uint8_t type=0, visibility=0;
};
struct ObjectRelocation {
  uint32_t section=0, symbol=0, type=0;
  uint64_t offset=0;
  int64_t addend=0;
  bool implicit_addend=false;
};
struct ObjectFile {
  ObjectFormat format;
  std::vector<ObjectSection> sections;
  std::vector<ObjectSymbol> symbols;
  std::vector<ObjectRelocation> relocations;
  std::string source;
};
struct ObjectLimits {
  uint64_t bytes=64*1024*1024;
  size_t sections=16384, symbols=1048576, relocations=1048576;
};
enum class RelocationKind { Absolute, PCRelative };
struct RelocationType {
  uint32_t type=0;
  std::string name;
  RelocationKind kind=RelocationKind::Absolute;
  uint32_t storage_bytes=0, bit_offset=0, bits=0;
  uint64_t scale=1;
  bool signed_value=false;
  int64_t pc_bias=0;
  std::optional<bool> implicit_addend_signed;
};
struct ObjectTarget {
  ObjectFormat format;
  uint64_t text_alignment=1;
  std::map<uint32_t,RelocationType> relocations;
  std::string architecture;
  // Explicit byte-preserving OS/processor section contracts. Indexed contents
  // and special allocation semantics need a separate adapter.
  std::vector<uint32_t> opaque_section_types;
};
// Authoritative ELF identity and relocation encodings come from tooling.object_file.
Result<ObjectTarget> object_target(const metacode::Architecture&);
Result<int> validate(const ObjectTarget&);
Result<int> validate(const ObjectFile&,const ObjectLimits& = {});
// Owning ELF32/ELF64 ET_REL ingestion and deterministic REL/RELA emission. No host structs
// are overlaid on input bytes; both byte orders use the same checked decoder.
Result<ObjectFile> load_elf(std::span<const uint8_t>,std::string_view source={},const ObjectLimits& = {});
Result<std::vector<uint8_t>> emit_elf(const ObjectFile&,const ObjectLimits& = {});
struct ArchiveMember { std::string name; ObjectFile object; };
struct ObjectArchive { std::vector<ArchiveMember> members; std::string source; };
struct ArchiveLimits { uint64_t bytes=64*1024*1024; size_t members=4096; ObjectLimits object; };
// Owning regular ar archives, including GNU/BSD long names and symbol tables.
Result<ObjectArchive> load_archive(std::span<const uint8_t>,std::string_view source={},const ArchiveLimits& = {});
Result<std::vector<uint8_t>> emit_archive(const ObjectArchive&,const ArchiveLimits& = {});
struct NamedRelocation { uint64_t offset; std::string kind, symbol; int64_t addend=0; };
Result<ObjectFile> code_object(const ObjectTarget&,std::span<const uint8_t>,std::string_view symbol,
                              std::span<const NamedRelocation> = {});
struct LinkedSection {
  size_t object; uint32_t section; std::string name;
  uint64_t address, offset, size, flags;
};
struct LinkOptions {
  uint64_t base_address=0, max_size=64*1024*1024;
  std::map<std::string,uint64_t> externals;
};
struct LinkedImage {
  uint64_t base_address=0;
  std::vector<uint8_t> bytes;
  std::vector<LinkedSection> sections;
  std::map<std::string,uint64_t> symbols;
};
// Layout follows input/section declaration order. The image owns bytes and has no
// executable mapping. Symbol addresses use the caller's explicit installation base.
Result<LinkedImage> link_objects(std::span<const ObjectFile>,const ObjectTarget&,const LinkOptions& = {});
Result<LinkedImage> link_objects(std::span<const ObjectFile* const>,const ObjectTarget&,const LinkOptions& = {});
// Extract members on demand, rescanning in declaration order to a fixed point.
// Undefined weak references do not cause extraction. Externals satisfy demand.
Result<LinkedImage> link_archives(std::span<const ObjectFile>,std::span<const ObjectArchive>,const ObjectTarget&,const LinkOptions& = {});
std::string print_object(const ObjectFile&);
}
