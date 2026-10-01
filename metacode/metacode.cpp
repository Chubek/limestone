#include "metacode.hpp"
#include <fstream>
#include <sstream>
#include <regex>
namespace limestone::metacode {
std::string Value::text() const {
  if (auto p=std::get_if<std::string>(&data)) return *p;
  if (auto p=std::get_if<int64_t>(&data)) return std::to_string(*p);
  if (auto p=std::get_if<bool>(&data)) return *p ? "true" : "false";
  if (auto p=std::get_if<Array>(&data)) { std::string s="["; for(size_t i=0;i<p->size();++i){if(i)s+=",";s+=(*p)[i].text();} return s+"]"; }
  std::string s="{"; bool first=true; for(auto&[k,v]:std::get<Object>(data)){if(!first)s+=",";first=false;s+=k+"="+v.text();} return s+"}";
}
namespace {
std::string strip_comments(std::string s){ return std::regex_replace(s,std::regex(R"(#[^\n]*)"),""); }
struct Block { std::string name, body; size_t end; };
std::optional<Block> next_block(const std::string& s,const std::string& keyword,size_t from){
  std::regex re("\\b"+keyword+R"(\s+([A-Za-z_][A-Za-z0-9_.-]*)\s*\{)");
  std::string rest=s.substr(std::min(from,s.size())); std::smatch m; if(!std::regex_search(rest,m,re)) return std::nullopt;
  size_t start=from+m.position(0), open=start+m.length(0)-1; int depth=0; bool str=false,esc=false;
  for(size_t i=open;i<s.size();++i){char c=s[i];if(str){if(esc)esc=false;else if(c=='\\')esc=true;else if(c=='"')str=false;continue;}if(c=='"'){str=true;continue;}if(c=='{')++depth;else if(c=='}'&&--depth==0)return Block{m[1].str(),s.substr(open+1,i-open-1),i+1};}
  return std::nullopt;
}
std::string quoted_field(const std::string& b,const char* n){std::regex re(std::string("\\b")+n+"\\s*=\\s*\"([^\"]*)\"");std::smatch m;return std::regex_search(b,m,re)?m[1].str():std::string{};}
}
Result<Architecture> parse_isa(std::string_view input){
  std::string s=strip_comments(std::string(input)); Architecture a;
  auto arch=next_block(s,"arch",0); if(!arch)return Result<Architecture>::err({Error::Code::Parse,"ISA contains no arch declaration"}); a.name=arch->name; a.family=quoted_field(s,"family");a.model=quoted_field(s,"model");a.version=quoted_field(s,"version");
  for(size_t pos=0;;){auto b=next_block(s,"regclass",pos);if(!b)break;std::regex er(R"(([A-Za-z_][A-Za-z0-9_.-]*)\s*\(\s*([0-9]+)\s*\)\s*=\s*([0-9]+))");for(std::sregex_iterator it(b->body.begin(),b->body.end(),er),ie;it!=ie;++it)a.registers.push_back({(*it)[1].str(),b->name,(uint32_t)std::stoul((*it)[2].str()),(uint32_t)std::stoul((*it)[3].str())});pos=b->end;}
  for(size_t pos=0;;){auto b=next_block(s,"op",pos);if(!b)break;Operation o;o.name=b->name;o.semantics=quoted_field(b->body,"semantics");a.operations.push_back(std::move(o));pos=b->end;}
  return Result<Architecture>::ok(std::move(a));
}
Result<Architecture> load_isa_file(const std::string& path){std::ifstream f(path);if(!f)return Result<Architecture>::err({Error::Code::NotFound,"cannot open "+path});std::stringstream ss;ss<<f.rdbuf();return parse_isa(ss.str());}
std::string dump(const Architecture&a){return "arch "+a.name+" { family=\""+a.family+"\"; model=\""+a.model+"\"; version=\""+a.version+"\"; }\nregisters: "+std::to_string(a.registers.size())+"\noperations: "+std::to_string(a.operations.size())+"\n";}
}
