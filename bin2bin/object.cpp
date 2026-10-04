#include "object.hpp"
#include <bit>
#include <limits>
#include <set>
#include <sstream>

namespace limestone::bin2bin {
namespace {
using Code=Error::Code;
using Value=metacode::Value;
using Fields=Value::Object;
[[noreturn]] void fail(std::string message,Code code=Code::InvalidArgument) { throw Error{code,std::move(message)}; }
uint64_t add(uint64_t a,uint64_t b) { if(b>UINT64_MAX-a)fail("object address/size overflow",Code::ResourceLimit);return a+b; }
bool aligned(uint64_t n) { return n&&std::has_single_bit(n); }
bool opaque_section(uint32_t type) { return type>=0x60000000&&type<=0x7fffffff; }
uint64_t align(uint64_t n,uint64_t a) { if(!aligned(a))fail("object alignment must be a power of two");return add(n,a-1)&~(a-1); }
void format(const ObjectFormat& f) {
  if(!f.machine||(f.byte_order!=ByteOrder::Little&&f.byte_order!=ByteOrder::Big))fail("invalid ELF machine or byte order");
  if(f.elf_class!=ElfClass::Elf32&&f.elf_class!=ElfClass::Elf64)fail("invalid ELF class");
}
struct Layout {
  bool narrow;
  uint32_t word, file_header, section_header, symbol, table_offset, flags_offset, sizes_offset;
  explicit Layout(ElfClass elf_class):narrow(elf_class==ElfClass::Elf32),word(narrow?4:8),
    file_header(narrow?52:64),section_header(narrow?40:64),symbol(narrow?16:24),
    table_offset(24+2*word),flags_offset(24+3*word),sizes_offset(28+3*word) {}
  uint32_t relocation(bool implicit) const { return word*(implicit?2:3); }
};
uint64_t mask(uint32_t bits) { return bits==64?UINT64_MAX:(uint64_t{1}<<bits)-1; }
struct Reader {
  std::span<const uint8_t> bytes;ByteOrder order;
  std::span<const uint8_t> span(uint64_t offset,uint64_t size) const {
    if(offset>bytes.size()||size>bytes.size()-offset)fail("ELF range exceeds input",Code::Parse);
    return bytes.subspan(static_cast<size_t>(offset),static_cast<size_t>(size));
  }
  uint64_t number(uint64_t offset,uint32_t width) const {
    auto data=span(offset,width);uint64_t n=0;
    for(uint32_t k=0;k<width;++k)n|=uint64_t(data[k])<<(8*(order==ByteOrder::Little?k:width-1-k));return n;
  }
  std::string string(uint64_t offset,uint64_t size,uint64_t index,uint64_t limit=UINT64_MAX) const {
    auto table=span(offset,size);if(index>=table.size())fail("ELF string offset out of range",Code::Parse);
    auto first=table.begin()+static_cast<ptrdiff_t>(index);auto end=std::find(first,table.end(),0);
    if(end==table.end())fail("unterminated ELF string",Code::Parse);if(uint64_t(end-first)>limit)fail("ELF expanded name byte limit exceeded",Code::ResourceLimit);return {first,end};
  }
};
void put(std::vector<uint8_t>& bytes,uint64_t offset,uint64_t value,uint32_t width,ByteOrder order) {
  if(width<8&&value>mask(width*8))fail("ELF field exceeds storage width",Code::ResourceLimit);
  for(uint32_t k=0;k<width;++k)bytes.at(static_cast<size_t>(offset+k))=uint8_t(value>>(8*(order==ByteOrder::Little?k:width-1-k)));
}
struct SectionHeader {
  uint32_t name=0,type=0,link=0,info=0;
  uint64_t flags=0,address=0,offset=0,size=0,alignment=0,entry_size=0;
};
SectionHeader header(const Reader& reader,uint64_t offset,const Layout& layout) {
  auto width=layout.word;
  return {uint32_t(reader.number(offset,4)),uint32_t(reader.number(offset+4,4)),
    uint32_t(reader.number(offset+8+4*width,4)),uint32_t(reader.number(offset+12+4*width,4)),
    reader.number(offset+8,width),reader.number(offset+8+width,width),reader.number(offset+8+2*width,width),
    reader.number(offset+8+3*width,width),reader.number(offset+16+4*width,width),reader.number(offset+16+5*width,width)};
}
void header(std::vector<uint8_t>& bytes,uint64_t offset,const SectionHeader& section,ByteOrder order,const Layout& layout) {
  auto width=layout.word;
  put(bytes,offset,section.name,4,order);put(bytes,offset+4,section.type,4,order);
  put(bytes,offset+8,section.flags,width,order);put(bytes,offset+8+width,section.address,width,order);
  put(bytes,offset+8+2*width,section.offset,width,order);put(bytes,offset+8+3*width,section.size,width,order);
  put(bytes,offset+8+4*width,section.link,4,order);put(bytes,offset+12+4*width,section.info,4,order);
  put(bytes,offset+16+4*width,section.alignment,width,order);put(bytes,offset+16+5*width,section.entry_size,width,order);
}
void limits(const ObjectFile& f,const ObjectLimits& l) {
  if(f.sections.size()>l.sections||f.symbols.size()>l.symbols||f.relocations.size()>l.relocations)fail("object count limit exceeded",Code::ResourceLimit);
  uint64_t size=0;for(auto& s:f.sections)size=add(size,add(s.size(),s.name.size()));for(auto& s:f.symbols)size=add(size,s.name.size());if(size>l.bytes)fail("object expanded byte limit exceeded",Code::ResourceLimit);
  if(f.sections.size()>=0xff00||f.symbols.size()>=UINT32_MAX)fail("object index limit exceeded",Code::ResourceLimit);
}
void check(const ObjectFile& f,const ObjectLimits& l) {
  format(f.format);limits(f,l);
  bool narrow=f.format.elf_class==ElfClass::Elf32;
  if(narrow&&f.symbols.size()>0xffffff)fail("ELF32 symbol index limit exceeded",Code::ResourceLimit);
  for(auto& s:f.sections) {
    if(narrow&&(s.flags>UINT32_MAX||s.alignment>UINT32_MAX||s.size()>UINT32_MAX||s.entry_size>UINT32_MAX))fail("ELF32 section field exceeds 32 bits");
    if(s.name.empty()||s.name.find('\0')!=std::string::npos||!aligned(s.alignment))fail("invalid object section name/alignment");
    if(s.type!=1&&s.type!=7&&s.type!=8&&!opaque_section(s.type))fail("object section type requires an adapter: "+std::to_string(s.type),Code::Unsupported);
    if(s.flags&2048)fail("compressed ELF sections require an adapter",Code::Unsupported);
    if((s.type==8&&!s.bytes.empty())||(s.type!=8&&s.zero_fill)||(s.entry_size&&s.size()%s.entry_size))fail("inconsistent object section storage/entry size");
  }
  for(auto& s:f.symbols) {
    if(narrow&&(s.value>UINT32_MAX||s.size>UINT32_MAX))fail("ELF32 symbol field exceeds 32 bits");
    if(s.name.find('\0')!=std::string::npos||s.binding<SymbolBinding::Local||s.binding>SymbolBinding::Weak||s.type>15||s.visibility>3)fail("invalid object symbol attributes");
    if(s.section==object_undefined) {
      if(s.binding==SymbolBinding::Local||s.name.empty()||s.value||s.size)fail("invalid undefined object symbol");
    }else if(s.section!=object_absolute) {
      if(s.section>=f.sections.size()||s.value>f.sections[s.section].size()||s.size>f.sections[s.section].size()-s.value)fail("object symbol exceeds section: "+s.name);
    }
    if(s.binding!=SymbolBinding::Local&&s.name.empty())fail("nonlocal object symbol needs a name");
  }
  for(auto& r:f.relocations) {
    if(r.section>=f.sections.size()||r.symbol>=f.symbols.size()||r.offset>=f.sections[r.section].size())fail("invalid object relocation reference/range");
    if(r.implicit_addend&&r.addend)fail("REL relocation cannot carry an explicit addend");
    if(narrow&&(r.type>UINT8_MAX||r.offset>UINT32_MAX||r.addend<INT32_MIN||r.addend>INT32_MAX))fail("ELF32 relocation field exceeds its encoding range");
  }
}
const Value& required(const Fields& fields,const std::string& key) {
  auto it=fields.find(key);if(it==fields.end())fail("missing object target field: "+key);return it->second;
}
uint64_t number(const Value& v) {
  if(auto n=std::get_if<uint64_t>(&v.data))return *n;
  if(auto n=std::get_if<int64_t>(&v.data);n&&*n>=0)return uint64_t(*n);fail("object target integer must be non-negative");
}
uint64_t number(const Fields& f,const std::string& k,uint64_t bound=UINT64_MAX) { auto n=number(required(f,k));if(n>bound)fail("object target field out of range: "+k);return n; }
std::string text(const Fields& fields,const std::string& key) { auto p=std::get_if<std::string>(&required(fields,key).data);if(!p)fail("object target field must be a string: "+key);return *p; }
const Fields& object(const Value& v) { auto p=std::get_if<Fields>(&v.data);if(!p)fail("object target contract must be an object");return *p; }
void known(const Fields& f,std::initializer_list<std::string_view> keys) {
  std::map<std::string,Value> ordered(f.begin(),f.end());for(auto& [key,v]:ordered)if(std::find(keys.begin(),keys.end(),key)==keys.end())fail("unsupported object target field: "+key,Code::Unsupported);
}
// Signed-magnitude arithmetic permits full 64-bit addresses without signed
// overflow, including negative displacements and the INT64_MIN endpoint.
struct Delta { bool negative=false;uint64_t magnitude=0; };
Delta difference(uint64_t a,uint64_t b) { return a>=b?Delta{false,a-b}:Delta{true,b-a}; }
Delta plus(Delta left,Delta right) {
  if(left.negative==right.negative)left.magnitude=add(left.magnitude,right.magnitude);
  else if(left.magnitude>=right.magnitude)left.magnitude-=right.magnitude;
  else {left.magnitude=right.magnitude-left.magnitude;left.negative=right.negative;}
  if(!left.magnitude)left.negative=false;return left;
}
Delta plus(Delta left,int64_t right) {
  return plus(left,Delta{right<0,right<0?uint64_t(-(right+1))+1:uint64_t(right)});
}
Delta implicit_addend(const ObjectSection& section,const ObjectRelocation& relocation,const RelocationType& type,ByteOrder order) {
  if(!type.implicit_addend_signed)fail("missing REL addend contract: "+type.name,Code::Unsupported);
  auto value=section.type==8?uint64_t{0}:Reader{section.bytes,order}.number(relocation.offset,type.storage_bytes);
  value=(value>>type.bit_offset)&mask(type.bits);
  bool negative=*type.implicit_addend_signed&&(value&(uint64_t{1}<<(type.bits-1)));
  auto magnitude=negative?((~value)&mask(type.bits))+1:value;
  if(magnitude>UINT64_MAX/type.scale)fail("REL addend scale overflow",Code::Conflict);
  return {negative,magnitude*type.scale};
}
uint64_t encode_delta(Delta n,const RelocationType& type) {
  if(n.magnitude%type.scale)fail("unaligned scaled relocation: "+type.name,Code::Conflict);n.magnitude/=type.scale;
  uint64_t bound=type.signed_value?(uint64_t{1}<<(type.bits-1)):mask(type.bits);
  if((n.negative&&!type.signed_value)||n.magnitude>bound||(!n.negative&&type.signed_value&&n.magnitude==bound))fail("relocation is out of encoding range: "+type.name,Code::Conflict);
  return (n.negative?uint64_t{0}-n.magnitude:n.magnitude)&mask(type.bits);
}
}
Result<int> validate(const ObjectTarget& target) {
  try {
    format(target.format);if(!aligned(target.text_alignment))fail("invalid object text alignment");std::set<std::string> names;
    if(target.format.elf_class==ElfClass::Elf32&&target.text_alignment>UINT32_MAX)fail("ELF32 text alignment exceeds 32 bits");
    std::set<uint32_t> sections;for(auto type:target.opaque_section_types)if(!opaque_section(type)||!sections.insert(type).second)fail("invalid/duplicate opaque object section type");
    for(auto& [id,t]:target.relocations) {
      if(target.format.elf_class==ElfClass::Elf32&&id>UINT8_MAX)fail("ELF32 relocation type exceeds 8 bits");
      if(id!=t.type||t.name.empty()||!names.insert(t.name).second||t.storage_bytes<1||t.storage_bytes>8||t.bits<1||t.bits>64||t.bit_offset>64-t.bits||t.bit_offset+t.bits>8*t.storage_bytes||!t.scale)fail("invalid object relocation encoding");
      if(t.kind!=RelocationKind::Absolute&&t.kind!=RelocationKind::PCRelative)fail("unknown object relocation kind");
      if(t.kind==RelocationKind::Absolute&&t.pc_bias)fail("absolute relocation cannot have a PC bias");
    }return Result<int>::ok(0);
  }catch(const Error& e){return Result<int>::err(e);}
}
Result<ObjectTarget> object_target(const metacode::Architecture& architecture) {
  try {
    auto it=architecture.fields.find("tooling");if(it==architecture.fields.end())fail("missing tooling.object_file contract",Code::Unsupported);
    auto& tooling=object(it->second);it=tooling.find("object_file");if(it==tooling.end())fail("missing tooling.object_file contract",Code::Unsupported);
    auto& f=object(it->second);known(f,{"format","version","machine","endianness","flags","osabi","abi_version","text_alignment","relocations","opaque_section_types"});
    auto container=text(f,"format");if((container!="elf32"&&container!="elf64")||number(f,"version")!=1)fail("unsupported object-file format/version",Code::Unsupported);
    ObjectTarget target;target.architecture=architecture.name;auto& out=target.format;out.machine=uint16_t(number(f,"machine",UINT16_MAX));out.flags=uint32_t(number(f,"flags",UINT32_MAX));out.osabi=uint8_t(number(f,"osabi",UINT8_MAX));out.abi_version=uint8_t(number(f,"abi_version",UINT8_MAX));
    out.elf_class=container=="elf32"?ElfClass::Elf32:ElfClass::Elf64;
    auto endian=text(f,"endianness");if(endian!="little"&&endian!="big")fail("invalid ELF byte order");out.byte_order=endian=="little"?ByteOrder::Little:ByteOrder::Big;target.text_alignment=number(f,"text_alignment");
    if(auto extra=f.find("opaque_section_types");extra!=f.end()){auto types=std::get_if<Value::Array>(&extra->second.data);if(!types)fail("opaque section types must be an array");for(auto& entry:*types){auto n=number(entry);if(n>UINT32_MAX)fail("opaque section type exceeds 32 bits");target.opaque_section_types.push_back(uint32_t(n));}}
    auto entries=std::get_if<Value::Array>(&required(f,"relocations").data);if(!entries)fail("object relocation encodings must be an array");
    for(auto& entry:*entries) {
      auto& fields=object(entry);known(fields,{"type","name","kind","storage_bytes","bit_offset","bits","scale","signed","pc_bias","implicit_addend_signed"});RelocationType t;
      t.type=uint32_t(number(fields,"type",UINT32_MAX));t.name=text(fields,"name");auto kind=text(fields,"kind");if(kind!="absolute"&&kind!="pc_relative")fail("unsupported object relocation expression: "+kind,Code::Unsupported);t.kind=kind=="absolute"?RelocationKind::Absolute:RelocationKind::PCRelative;
      t.storage_bytes=uint32_t(number(fields,"storage_bytes",8));t.bit_offset=uint32_t(number(fields,"bit_offset",63));t.bits=uint32_t(number(fields,"bits",64));t.scale=number(fields,"scale");auto sign=std::get_if<bool>(&required(fields,"signed").data);if(!sign)fail("object relocation signed must be Boolean");t.signed_value=*sign;
      if(auto implicit=fields.find("implicit_addend_signed");implicit!=fields.end()){auto value=std::get_if<bool>(&implicit->second.data);if(!value)fail("REL addend signedness must be Boolean");t.implicit_addend_signed=*value;}
      if(auto bias=fields.find("pc_bias");bias!=fields.end()){auto value=std::get_if<int64_t>(&bias->second.data);if(value)t.pc_bias=*value;else {auto n=number(bias->second);if(n>INT64_MAX)fail("object PC bias exceeds signed range");t.pc_bias=int64_t(n);}}
      if(!target.relocations.emplace(t.type,std::move(t)).second)fail("duplicate object relocation type",Code::Conflict);
    }
    auto valid=validate(target);if(!valid)throw valid.error();return Result<ObjectTarget>::ok(std::move(target));
  }catch(const Error& e){return Result<ObjectTarget>::err({e.code,architecture.source.file+":"+std::to_string(architecture.source.line)+": object target: "+e.message});}
}
Result<int> validate(const ObjectFile& file,const ObjectLimits& l) {
  try {check(file,l);return Result<int>::ok(0);}catch(const Error& e){return Result<int>::err(e);}
}
Result<ObjectFile> load_elf(std::span<const uint8_t> bytes,std::string_view source,const ObjectLimits& l) {
  try {
    if(bytes.size()>l.bytes)fail("ELF input byte limit exceeded",Code::ResourceLimit);
    if(bytes.size()<16||bytes[0]!=0x7f||bytes[1]!='E'||bytes[2]!='L'||bytes[3]!='F')fail("invalid ELF header",Code::Parse);
    if(bytes[4]!=1&&bytes[4]!=2)fail("unsupported ELF class",Code::Unsupported);if(bytes[5]!=1&&bytes[5]!=2)fail("invalid ELF byte order",Code::Parse);
    auto elf_class=static_cast<ElfClass>(bytes[4]);Layout layout(elf_class);
    Reader r{bytes,bytes[5]==1?ByteOrder::Little:ByteOrder::Big};r.span(0,layout.file_header);
    if(bytes[6]!=1||r.number(20,4)!=1||r.number(layout.sizes_offset,2)!=layout.file_header)fail("invalid ELF version/header size",Code::Parse);
    if(r.number(16,2)!=1)fail("ELF input must be a relocatable object",Code::Unsupported);
    if(r.number(24,layout.word)||r.number(24+layout.word,layout.word)||r.number(layout.sizes_offset+2,2)||r.number(layout.sizes_offset+4,2))fail("relocatable ELF has an entry point or program headers",Code::Unsupported);
    auto table=r.number(layout.table_offset,layout.word),count=r.number(layout.sizes_offset+8,2),strings=r.number(layout.sizes_offset+10,2);
    if(!count||count>=0xff00||strings==0xffff)fail("extended ELF section indexes require an adapter",Code::Unsupported);
    if(count>add(add(l.sections,4),l.relocations))fail("ELF section count limit exceeded",Code::ResourceLimit);
    if(r.number(layout.sizes_offset+6,2)!=layout.section_header||table<layout.file_header||table%layout.word||strings>=count||!strings)fail("invalid ELF section table",Code::Parse);r.span(table,count*layout.section_header);
    if(std::any_of(bytes.begin()+9,bytes.begin()+16,[](auto n){return n!=0;}))fail("unsupported ELF identification padding",Code::Unsupported);
    ObjectFile file;file.source=source;file.format={uint16_t(r.number(18,2)),r.order,uint32_t(r.number(layout.flags_offset,4)),bytes[7],bytes[8],elf_class};
    std::vector<SectionHeader> sections;for(uint64_t k=0;k<count;++k)sections.push_back(header(r,table+k*layout.section_header,layout));
    auto& zero=sections[0];if(zero.type||zero.name||zero.flags||zero.address||zero.offset||zero.size||zero.link||zero.info||zero.alignment||zero.entry_size)fail("invalid null ELF section",Code::Parse);
    auto& str=sections[strings];if(str.type!=3||!str.size||r.number(str.offset,1)!=0)fail("invalid ELF section-name table",Code::Parse);
    std::vector<std::optional<uint32_t>> section_ids(count);std::optional<size_t> symbols;
    uint64_t expanded=0;auto name=[&](const SectionHeader& strings,uint64_t index){auto text=r.string(strings.offset,strings.size,index,l.bytes-expanded);expanded=add(expanded,text.size());return text;};
    std::vector<std::pair<uint64_t,uint64_t>> ranges{{0,layout.file_header},{table,table+count*layout.section_header}};
    for(size_t k=1;k<count;++k){auto& h=sections[k];if(h.type!=8&&h.size){r.span(h.offset,h.size);ranges.emplace_back(h.offset,h.offset+h.size);}}
    std::sort(ranges.begin(),ranges.end());for(size_t k=1;k<ranges.size();++k)if(ranges[k].first<ranges[k-1].second)fail("overlapping ELF sections/header tables",Code::Parse);
    for(size_t k=1;k<count;++k) {
      auto& h=sections[k];if(h.address||!aligned(h.alignment?h.alignment:1))fail("invalid relocatable ELF section address/alignment",Code::Parse);
      (void)r.string(str.offset,str.size,h.name,l.bytes);
      if(h.type!=8&&h.size&&h.offset%(h.alignment?h.alignment:1))fail("misaligned ELF section storage",Code::Parse);
      if(h.type!=8){r.span(h.offset,h.size);if(h.size&&h.offset<layout.file_header)fail("ELF section overlaps header",Code::Parse);}
      if(h.type==2){if(symbols)fail("multiple ELF symbol tables require an adapter",Code::Unsupported);if(h.flags)fail("allocated/flagged ELF symbol table",Code::Unsupported);symbols=k;continue;}
      if(h.type==3){if(h.flags||h.link||h.info||h.entry_size)fail("unsupported ELF string-table metadata",Code::Unsupported);continue;}
      if(h.type==4||h.type==9){if(h.flags&~uint64_t{64})fail("unsupported ELF relocation metadata",Code::Unsupported);continue;}
      if(h.type!=1&&h.type!=7&&h.type!=8&&!opaque_section(h.type))fail("unsupported ELF section type: "+std::to_string(h.type),Code::Unsupported);
      if(h.link||h.info)fail("linked ELF section metadata requires an adapter",Code::Unsupported);
      if(file.sections.size()>=l.sections)fail("ELF content section count limit exceeded",Code::ResourceLimit);
      ObjectSection s{name(str,h.name),h.type,h.flags,h.alignment?h.alignment:1,h.entry_size};
      expanded=add(expanded,h.size);if(expanded>l.bytes)fail("ELF expanded section byte limit exceeded",Code::ResourceLimit);
      if(h.type==8)s.zero_fill=h.size;else {auto data=r.span(h.offset,h.size);s.bytes.assign(data.begin(),data.end());}
      section_ids[k]=uint32_t(file.sections.size());file.sections.push_back(std::move(s));
    }
    std::vector<std::optional<uint32_t>> symbol_ids;
    if(symbols) {
      auto& h=sections[*symbols];if(h.link>=count||sections[h.link].type!=3||h.entry_size!=layout.symbol||h.size%layout.symbol||h.size<layout.symbol||!h.info||h.info>h.size/layout.symbol)fail("invalid ELF symbol table",Code::Parse);
      auto& names=sections[h.link];if(!names.size||r.number(names.offset,1))fail("invalid ELF symbol strings",Code::Parse);
      auto total=h.size/layout.symbol;if(total-1>l.symbols)fail("ELF symbol count limit exceeded",Code::ResourceLimit);symbol_ids.resize(total);
      for(uint64_t k=0;k<total;++k) {
        auto p=h.offset+k*layout.symbol,attributes=p+(layout.narrow?12:4);
        auto name=r.number(p,4),info=r.number(attributes,1),other=r.number(attributes+1,1),section=r.number(attributes+2,2),value=r.number(p+(layout.narrow?4:8),layout.word),size=r.number(p+(layout.narrow?8:16),layout.word);
        if(!k){if(name||info||other||section||value||size)fail("invalid null ELF symbol",Code::Parse);continue;}
        if((info>>4)>2||other>3)fail("unsupported ELF symbol binding/visibility",Code::Unsupported);
        if(((info>>4)==0)!=(k<h.info))fail("ELF local symbol partition is inconsistent",Code::Parse);
        uint32_t id=object_undefined;
        if(section==0xfff1)id=object_absolute;
        else if(section){if(section>=count||!section_ids[section])fail("ELF symbol references unsupported section",Code::Unsupported);id=*section_ids[section];}
        ObjectSymbol s{r.string(names.offset,names.size,name,l.bytes-expanded),id,value,size,static_cast<SymbolBinding>(info>>4),uint8_t(info&15),uint8_t(other)};expanded=add(expanded,s.name.size());
        symbol_ids[k]=uint32_t(file.symbols.size());file.symbols.push_back(std::move(s));
      }
    }
    for(size_t k=1;k<count;++k)if(sections[k].type==3&&k!=strings&&(!symbols||k!=sections[*symbols].link))fail("unreferenced ELF string table requires an adapter",Code::Unsupported);
    for(size_t k=1;k<count;++k)if(sections[k].type==4||sections[k].type==9) {
      auto& h=sections[k];bool implicit=h.type==9;auto entry_size=layout.relocation(implicit);
      if(!symbols||h.link!=*symbols||h.info>=count||!section_ids[h.info]||h.entry_size!=entry_size||h.size%entry_size)fail("invalid ELF relocation section",Code::Parse);
      if(h.size/entry_size>l.relocations-file.relocations.size())fail("ELF relocation count limit exceeded",Code::ResourceLimit);
      for(uint64_t n=0;n<h.size/entry_size;++n) {
        auto p=h.offset+n*entry_size,offset=r.number(p,layout.word),info=r.number(p+layout.word,layout.word),symbol=info>>(layout.narrow?8:32);
        if(symbol>=symbol_ids.size()||!symbol_ids[symbol])fail("ELF relocation references missing/null symbol",Code::Parse);
        int64_t addend=0;if(!implicit){auto encoded=r.number(p+2*layout.word,layout.word);addend=layout.narrow?int64_t(std::bit_cast<int32_t>(uint32_t(encoded))):std::bit_cast<int64_t>(encoded);}
        file.relocations.push_back({*section_ids[h.info],*symbol_ids[symbol],uint32_t(info&(layout.narrow?UINT8_MAX:UINT32_MAX)),offset,addend,implicit});
      }
    }
    check(file,l);return Result<ObjectFile>::ok(std::move(file));
  }catch(const Error& e){return Result<ObjectFile>::err({e.code,(source.empty()?"":std::string(source)+": ")+e.message});}
}
Result<std::vector<uint8_t>> emit_elf(const ObjectFile& file,const ObjectLimits& l) {
  try {
    check(file,l);auto order=file.format.byte_order;Layout layout(file.format.elf_class);
    struct Output {std::string name;SectionHeader header;std::vector<uint8_t> bytes;};
    std::vector<Output> sections(1);
    for(auto& s:file.sections)sections.push_back({s.name,{0,s.type,0,0,s.flags,0,0,s.size(),s.alignment,s.entry_size},s.bytes});
    auto symtab=uint32_t(sections.size()),strtab=symtab+1;
    std::vector<uint8_t> names{0};auto string=[&](std::vector<uint8_t>& table,std::string_view name)->uint32_t{if(add(add(table.size(),name.size()),1)>UINT32_MAX)fail("ELF string table overflow",Code::ResourceLimit);auto index=uint32_t(table.size());table.insert(table.end(),name.begin(),name.end());table.push_back(0);return index;};
    std::vector<uint32_t> ids(file.symbols.size());std::vector<const ObjectSymbol*> symbols{nullptr};
    for(auto binding:{SymbolBinding::Local,SymbolBinding::Global,SymbolBinding::Weak})for(size_t k=0;k<file.symbols.size();++k)if(file.symbols[k].binding==binding){ids[k]=uint32_t(symbols.size());symbols.push_back(&file.symbols[k]);}
    uint32_t first=1;while(first<symbols.size()&&symbols[first]->binding==SymbolBinding::Local)++first;
    std::vector<uint8_t> symbytes(symbols.size()*layout.symbol);
    for(size_t k=1;k<symbols.size();++k) {
      auto& s=*symbols[k];auto p=k*layout.symbol,attributes=p+(layout.narrow?12:4);put(symbytes,p,string(names,s.name),4,order);put(symbytes,attributes,(uint32_t(s.binding)<<4)|s.type,1,order);put(symbytes,attributes+1,s.visibility,1,order);
      put(symbytes,attributes+2,s.section==object_undefined?0:s.section==object_absolute?0xfff1:s.section+1,2,order);put(symbytes,p+(layout.narrow?4:8),s.value,layout.word,order);put(symbytes,p+(layout.narrow?8:16),s.size,layout.word,order);
    }
    sections.push_back({".symtab",{0,2,strtab,first,0,0,0,symbytes.size(),layout.word,layout.symbol},std::move(symbytes)});
    sections.push_back({".strtab",{0,3,0,0,0,0,0,names.size(),1,0},std::move(names)});
    std::map<std::pair<uint32_t,bool>,std::vector<const ObjectRelocation*>> groups;
    for(auto& relocation:file.relocations)groups[{relocation.section,relocation.implicit_addend}].push_back(&relocation);
    for(auto& [key,rels]:groups) {
      auto [section,implicit]=key;auto entry_size=layout.relocation(implicit);
      std::vector<uint8_t> data(rels.size()*entry_size);
      for(size_t index=0;index<rels.size();++index){auto& relocation=*rels[index];auto offset=index*entry_size;
        put(data,offset,relocation.offset,layout.word,order);put(data,offset+layout.word,(uint64_t(ids[relocation.symbol])<<(layout.narrow?8:32))|relocation.type,layout.word,order);
        if(!implicit)put(data,offset+2*layout.word,layout.narrow?uint32_t(relocation.addend):std::bit_cast<uint64_t>(relocation.addend),layout.word,order);
      }
      sections.push_back({std::string(implicit?".rel":".rela")+file.sections[section].name,{0,implicit?9u:4u,symtab,section+1,0,0,0,data.size(),layout.word,entry_size},std::move(data)});
    }
    auto strings=uint32_t(sections.size());sections.push_back({".shstrtab",{0,3,0,0,0,0,0,0,1,0},{}});
    if(sections.size()>=0xff00)fail("ELF section count exceeds direct index range",Code::ResourceLimit);
    std::vector<uint8_t> shnames{0};for(size_t k=1;k<sections.size();++k)sections[k].header.name=string(shnames,sections[k].name);sections.back().header.size=shnames.size();sections.back().bytes=std::move(shnames);
    uint64_t size=layout.file_header;for(size_t k=1;k<sections.size();++k){auto& s=sections[k];size=align(size,s.header.alignment);s.header.offset=size;if(s.header.type!=8)size=add(size,s.bytes.size());if(size>l.bytes)fail("ELF output byte limit exceeded",Code::ResourceLimit);}
    auto table=align(size,layout.word);size=add(table,sections.size()*layout.section_header);if(size>l.bytes||size>SIZE_MAX||(layout.narrow&&size>UINT32_MAX))fail("ELF output byte limit exceeded",Code::ResourceLimit);std::vector<uint8_t> bytes(static_cast<size_t>(size));
    bytes[0]=0x7f;bytes[1]='E';bytes[2]='L';bytes[3]='F';bytes[4]=uint8_t(file.format.elf_class);bytes[5]=order==ByteOrder::Little?1:2;bytes[6]=1;bytes[7]=file.format.osabi;bytes[8]=file.format.abi_version;
    put(bytes,16,1,2,order);put(bytes,18,file.format.machine,2,order);put(bytes,20,1,4,order);put(bytes,layout.table_offset,table,layout.word,order);put(bytes,layout.flags_offset,file.format.flags,4,order);put(bytes,layout.sizes_offset,layout.file_header,2,order);put(bytes,layout.sizes_offset+6,layout.section_header,2,order);put(bytes,layout.sizes_offset+8,sections.size(),2,order);put(bytes,layout.sizes_offset+10,strings,2,order);
    for(size_t k=1;k<sections.size();++k){auto& s=sections[k];std::copy(s.bytes.begin(),s.bytes.end(),bytes.begin()+static_cast<ptrdiff_t>(s.header.offset));header(bytes,table+k*layout.section_header,s.header,order,layout);}
    return Result<std::vector<uint8_t>>::ok(std::move(bytes));
  }catch(const Error& e){return Result<std::vector<uint8_t>>::err(e);}
}
Result<ObjectFile> code_object(const ObjectTarget& target,std::span<const uint8_t> bytes,std::string_view symbol,std::span<const NamedRelocation> rels) {
  try {
    auto valid=validate(target);if(!valid)throw valid.error();if(symbol.empty()||symbol.find('\0')!=std::string_view::npos)fail("code object needs a nonempty symbol");
    if(bytes.size()>ObjectLimits{}.bytes||rels.size()>ObjectLimits{}.relocations)fail("code object input limit exceeded",Code::ResourceLimit);
    ObjectFile file;file.format=target.format;ObjectSection text{".text",1,6,target.text_alignment};text.bytes.assign(bytes.begin(),bytes.end());file.sections.push_back(std::move(text));file.symbols.push_back({std::string(symbol),0,0,bytes.size(),SymbolBinding::Global,2});
    std::map<std::string,uint32_t> symbols{{std::string(symbol),0}};
    for(auto& r:rels) {
      auto type=std::find_if(target.relocations.begin(),target.relocations.end(),[&](auto& p){return p.second.name==r.kind;});if(type==target.relocations.end())fail("unknown named object relocation: "+r.kind,Code::Unsupported);
      if(r.symbol.empty()||r.symbol.find('\0')!=std::string::npos)fail("object relocation needs a symbol");
      auto [it,added]=symbols.emplace(r.symbol,uint32_t(file.symbols.size()));if(added)file.symbols.push_back({r.symbol});
      file.relocations.push_back({0,it->second,type->first,r.offset,r.addend});
      if(r.offset>bytes.size()||type->second.storage_bytes>bytes.size()-r.offset)fail("object relocation exceeds encoded code");
    }
    check(file,{});return Result<ObjectFile>::ok(std::move(file));
  }catch(const Error& e){return Result<ObjectFile>::err(e);}
}
Result<LinkedImage> link_objects(std::span<const ObjectFile> files,const ObjectTarget& target,const LinkOptions& options) {
  if(files.size()>16384)return Result<LinkedImage>::err({Code::ResourceLimit,"link object count limit exceeded"});
  std::vector<const ObjectFile*> views;views.reserve(files.size());for(auto& f:files)views.push_back(&f);return link_objects(views,target,options);
}
Result<LinkedImage> link_objects(std::span<const ObjectFile* const> files,const ObjectTarget& target,const LinkOptions& options) {
  try {
    auto valid=validate(target);if(!valid)throw valid.error();if(files.empty())fail("link needs at least one object");if(files.size()>16384)fail("link object count limit exceeded",Code::ResourceLimit);
    if(options.externals.size()>ObjectLimits{}.symbols)fail("link external symbol count limit exceeded",Code::ResourceLimit);
    for(auto& [name,address]:options.externals)if(name.empty()||name.find('\0')!=std::string::npos)fail("invalid external object symbol name");
    LinkedImage image;image.base_address=options.base_address;uint64_t cursor=options.base_address;
    std::map<std::pair<size_t,uint32_t>,uint64_t> addresses;
    struct Definition {size_t object;uint32_t symbol;};std::map<std::string,Definition> globals;
    uint64_t section_count=0,symbol_count=0,relocation_count=0;
    for(size_t k=0;k<files.size();++k) {
      if(!files[k])fail("null link object");auto& f=*files[k];check(f,{});if(f.format!=target.format)fail("incompatible ELF object target: "+f.source,Code::Conflict);
      section_count=add(section_count,f.sections.size());symbol_count=add(symbol_count,f.symbols.size());relocation_count=add(relocation_count,f.relocations.size());
      if(section_count>ObjectLimits{}.sections||symbol_count>ObjectLimits{}.symbols||relocation_count>ObjectLimits{}.relocations)fail("aggregate link count limit exceeded",Code::ResourceLimit);
      for(uint32_t n=0;n<f.sections.size();++n) {
        auto& s=f.sections[n];if(!(s.flags&2))continue;
        if(opaque_section(s.type)&&std::find(target.opaque_section_types.begin(),target.opaque_section_types.end(),s.type)==target.opaque_section_types.end())fail("missing opaque section contract: "+std::to_string(s.type),Code::Unsupported);
        if(s.flags&~uint64_t{7})fail("allocated ELF section flags require a linker adapter: "+s.name,Code::Unsupported);
        cursor=align(cursor,s.alignment);auto end=add(cursor,s.size());if(end-options.base_address>options.max_size||end-options.base_address>SIZE_MAX)fail("linked image byte limit exceeded",Code::ResourceLimit);
        auto offset=cursor-options.base_address;image.bytes.resize(static_cast<size_t>(end-options.base_address));if(s.type!=8)std::copy(s.bytes.begin(),s.bytes.end(),image.bytes.begin()+static_cast<ptrdiff_t>(offset));
        addresses[{k,n}]=cursor;image.sections.push_back({k,n,s.name,cursor,offset,s.size(),s.flags});cursor=end;
      }
      for(uint32_t n=0;n<f.symbols.size();++n) {
        auto& s=f.symbols[n];if(s.type!=0&&s.type!=1&&s.type!=2&&s.type!=3&&s.type!=4)fail("ELF symbol type requires a linker adapter: "+s.name,Code::Unsupported);
        if(s.section==object_undefined||s.binding==SymbolBinding::Local)continue;
        auto [it,added]=globals.emplace(s.name,Definition{k,n});if(!added) {
          auto& old=files[it->second.object]->symbols[it->second.symbol];
          if(old.binding==SymbolBinding::Global&&s.binding==SymbolBinding::Global)fail("multiple strong definitions: "+s.name,Code::Conflict);
          if(old.binding==SymbolBinding::Weak&&s.binding==SymbolBinding::Global)it->second={k,n};
        }
      }
    }
    auto defined=[&](size_t k,const ObjectSymbol& s)->uint64_t {
      if(s.section==object_absolute)return s.value;auto it=addresses.find({k,s.section});if(it==addresses.end())fail("symbol has no allocated section: "+s.name,Code::Unsupported);return add(it->second,s.value);
    };
    auto resolve=[&](size_t k,const ObjectSymbol& s)->uint64_t {
      if(s.binding==SymbolBinding::Local)return defined(k,s);
      if(auto it=globals.find(s.name);it!=globals.end()){auto d=it->second;return defined(d.object,files[d.object]->symbols[d.symbol]);}
      if(s.visibility)fail("hidden/internal undefined symbol: "+s.name,Code::Conflict);
      if(auto it=options.externals.find(s.name);it!=options.externals.end())return it->second;
      if(s.binding==SymbolBinding::Weak)return 0;fail("unresolved object symbol: "+s.name,Code::NotFound);
    };
    for(auto& [name,d]:globals){auto& s=files[d.object]->symbols[d.symbol];if(s.section==object_absolute||addresses.contains({d.object,s.section}))image.symbols[name]=defined(d.object,s);}
    std::map<uint64_t,uint8_t> patched;
    for(size_t k=0;k<files.size();++k)for(auto& r:files[k]->relocations) {
      auto address=addresses.find({k,r.section});if(address==addresses.end())continue;
      auto type=target.relocations.find(r.type);if(type==target.relocations.end())fail("missing ELF relocation encoding: "+std::to_string(r.type),Code::Unsupported);auto& t=type->second;
      if(t.storage_bytes>files[k]->sections[r.section].size()-r.offset)fail("ELF relocation storage exceeds section");
      auto place=add(address->second,r.offset),symbol=resolve(k,files[k]->symbols[r.symbol]);
      Delta delta{false,symbol};if(t.kind==RelocationKind::PCRelative){auto pc=plus({false,place},t.pc_bias);if(pc.negative)fail("relocation PC base is negative",Code::Conflict);delta=difference(symbol,pc.magnitude);}
      delta=r.implicit_addend?plus(delta,implicit_addend(files[k]->sections[r.section],r,t,target.format.byte_order)):plus(delta,r.addend);
      auto bits=encode_delta(delta,t),bitmask=mask(t.bits)<<t.bit_offset,offset=place-image.base_address;
      for(uint32_t n=0;n<t.storage_bytes;++n) {
        auto shift=8*(target.format.byte_order==ByteOrder::Little?n:t.storage_bytes-1-n);auto m=uint8_t(bitmask>>shift);auto& used=patched[offset+n];if(used&m)fail("overlapping object relocation fields",Code::Conflict);used|=m;
      }
      Reader reader{image.bytes,target.format.byte_order};auto word=reader.number(offset,t.storage_bytes);word=(word&~bitmask)|(bits<<t.bit_offset);put(image.bytes,offset,word,t.storage_bytes,target.format.byte_order);
    }
    return Result<LinkedImage>::ok(std::move(image));
  }catch(const Error& e){return Result<LinkedImage>::err(e);}
}
std::string print_object(const ObjectFile& file) {
  std::ostringstream out;out<<(file.format.elf_class==ElfClass::Elf32?"ELF32":"ELF64")<<" machine "<<file.format.machine<<" "<<(file.format.byte_order==ByteOrder::Little?"little":"big")<<" flags "<<file.format.flags<<"\n";
  for(size_t k=0;k<file.sections.size();++k){auto& s=file.sections[k];out<<"section "<<k<<" "<<s.name<<" size "<<s.size()<<" align "<<s.alignment<<" flags "<<s.flags<<" type "<<s.type<<"\n";}
  for(size_t k=0;k<file.symbols.size();++k){auto& s=file.symbols[k];out<<"symbol "<<k<<" "<<s.name<<" section ";if(s.section==object_undefined)out<<"undefined";else if(s.section==object_absolute)out<<"absolute";else out<<s.section;out<<" value "<<s.value<<" size "<<s.size<<" binding "<<int(s.binding)<<" type "<<int(s.type)<<"\n";}
  for(auto& r:file.relocations){out<<"relocation section "<<r.section<<" offset "<<r.offset<<" type "<<r.type<<" symbol "<<r.symbol<<" addend ";if(r.implicit_addend)out<<"implicit";else out<<r.addend;out<<"\n";}return out.str();
}
}
