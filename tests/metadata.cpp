#include "test.hpp"
#include "metacode.hpp"
#include <filesystem>

int main(int argc,char** argv) {return test_main([&]{
  using namespace limestone;
  auto a=take(metacode::parse_isa(R"(arch toy { word_size = 64; }
profile { family = "test"; model = "v1"; version = "2"; }
regclass G { r0(64)=0, r1(32)=1, }
alias acc = r0;
encoding E { width = 8; base = 0xff; }
op NOP { encoding = E; operands = ; semantics = (nop);
  tooling = { unknown = { preserved = [1, true, "x", { nested = 7; }]; }; }
}
)","toy.isa"));
  CHECK(a.name=="toy"&&a.family=="test"&&a.model=="v1"&&a.version=="2");
  CHECK(a.registers.size()==2&&a.registers[1].width==32);
  CHECK(a.aliases.at("acc")=="r0");CHECK(a.encodings.at("E").at("base").text()=="255");
  CHECK(a.operations.size()==1&&a.operations[0].semantics=="(nop)");
  CHECK(a.operations[0].source.file=="toy.isa"&&a.operations[0].source.line==6);
  const auto& tooling=std::get<metacode::Value::Object>(a.operations[0].fields.at("tooling").data);
  CHECK(tooling.at("unknown").text().find("nested=7")!=std::string::npos);
  CHECK(a.operations[0].fields.at("operands").text().empty());
  auto escaped=take(metacode::parse_isa(R"ISA(arch strings { x = "a\"b\\c\n"; } op eor { semantics = "(intrinsic \"eor\" x y)"; })ISA"));
  CHECK(escaped.fields.at("x").text()=="a\"b\\c\n");CHECK(escaped.operations[0].semantics=="(intrinsic \"eor\" x y)");
  auto roundtrip=take(metacode::parse_isa("arch a { x = "+metacode::Value(a.fields).text()+"; }"));
  CHECK(std::get<metacode::Value::Object>(roundtrip.fields.at("x").data).at("word_size").text()=="64");
  for(auto bad:{"", "arch a { x = 1; x = 2; }", "arch a {} arch b {}", "arch a {} regclass G { r0(0)=0, }", "arch a {} op x {} op x {}", "arch a { profile = 3; }", "arch a { x = 18446744073709551616; }", "arch a { x = [1,; }", "arch a { x = \"unterminated; }"})
    fails(metacode::parse_isa(bad),Error::Code::Parse);
  CHECK(argc==2);
  std::vector<std::filesystem::path> corpus;
  for(auto& entry:std::filesystem::directory_iterator(argv[1]))if(entry.path().extension()==".isa"&&entry.path().filename().string().front()!='.')corpus.push_back(entry.path());
  std::sort(corpus.begin(),corpus.end());size_t loaded=0;
  for(auto& path:corpus) {
    if(path.filename()=="dsl.lua.isa"){fails(metacode::load_isa_file(path.string()),Error::Code::Parse);continue;}
    auto isa=take(metacode::load_isa_file(path.string()));
    CHECK(!isa.name.empty()&&!isa.operations.empty());
    const auto& profile=std::get<metacode::Value::Object>(isa.fields.at("profile").data);
    CHECK(std::stoull(profile.at("instruction_count").text())==isa.operations.size());
    const auto& global=std::get<metacode::Value::Object>(isa.fields.at("tooling").data);
    CHECK(std::get<metacode::Value::Object>(global.at("bin2bin").data).at("schema_version").text()=="1");
    for(auto& op:isa.operations){CHECK(!op.semantics.empty());CHECK(std::get<metacode::Value::Object>(op.fields.at("tooling").data).contains("binary_translation"));}
    ++loaded;
  }
  CHECK(loaded==23);
});}
