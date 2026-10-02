#include "test.hpp"
#include "bin2bin.hpp"
#include <filesystem>

int main(int argc,char** argv){return test_main([&]{
  using namespace limestone;using namespace bin2bin;CHECK(argc==3);
  auto src=take(from_metacode(take(metacode::load_isa_file(std::string(argv[1])+"/byte-source.isa"))));
  auto dst=take(from_metacode(take(metacode::load_isa_file(std::string(argv[1])+"/byte-target.isa"))));
  std::vector<uint8_t> bytes{1,1,1};
  CHECK(take(translate(src,src,bytes))==bytes);
  auto translated=take(translate(src,dst,bytes));CHECK(translated==std::vector<uint8_t>({9,9,9}));
  CHECK(take(translate(dst,src,translated))==bytes);
  auto instructions=take(decode(src,std::vector<uint8_t>{1,255},16));CHECK(instructions[0].address==16&&instructions[1].status==Status::Unsupported);
  CHECK(disassemble(instructions).find("11: .byte 0xff ; unsupported")!=std::string::npos);
  CHECK(take(lift(src,bytes))[0].semantics=="(set counter (add counter 1))");
  CHECK(take(decompile(src,bytes,16)).find("10: (set counter")!=std::string::npos);
  fails(translate(src,dst,std::vector<uint8_t>{255}),Error::Code::Unsupported);
  fails(decode(src,bytes,std::numeric_limits<uint64_t>::max()),Error::Code::InvalidArgument);
  // Numeric opcode coincidence must not imply semantic equivalence.
  auto wrong=src;wrong.name="wrong";wrong.semantics[1]="(set counter (sub counter 1))";
  fails(translate(src,wrong,bytes),Error::Code::Unsupported);
  auto ambiguous=dst;ambiguous.status[9]=Status::Ambiguous;fails(translate(src,ambiguous,bytes),Error::Code::Unsupported);
  auto domain=dst;domain.execution_domain="native_machine_code";fails(translate(src,domain,bytes),Error::Code::Unsupported);
  auto no_semantics=dst;no_semantics.semantics.clear();fails(translate(src,no_semantics,bytes),Error::Code::Unsupported);
  auto malformed=src;malformed.semantics[1]="(unterminated";fails(lift(malformed,bytes),Error::Code::Parse);
  TranslationCache memory;CHECK(take(translate(src,dst,bytes,&memory))==translated);CHECK(memory.entries.size()==1);
  auto changed=dst;changed.version="2";changed.opcodes.erase(9);changed.semantics.erase(9);changed.opcodes[10]="inc2";changed.semantics[10]=src.semantics.at(1);
  CHECK(take(translate(src,changed,bytes,&memory))==std::vector<uint8_t>({10,10,10}));CHECK(memory.entries.size()==2);
  TranslationOptions options;options.rule_version="changed";take(translate(src,dst,bytes,&memory,options));CHECK(memory.entries.size()==3);
#ifdef LIMESTONE_TEST_LMDB
  std::filesystem::remove_all(argv[2]);
  {TranslationCache persistent;take(open_cache(persistent,argv[2]));CHECK(take(translate(src,dst,bytes,&persistent))==translated);}
  {TranslationCache persistent;take(open_cache(persistent,argv[2]));CHECK(take(translate(src,dst,bytes,&persistent))==translated);CHECK(persistent.entries.size()==1);CHECK(take(translate(src,changed,bytes,&persistent))==std::vector<uint8_t>({10,10,10}));}
  std::filesystem::remove_all(argv[2]);
#else
  TranslationCache persistent;fails(open_cache(persistent,argv[2]),Error::Code::Unsupported);
#endif
});}
