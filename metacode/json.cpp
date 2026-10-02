#include "json.hpp"
#include <charconv>
#include <map>

namespace limestone::metacode {
namespace {
class Reader {
  std::string_view source,file;size_t position=0;
  [[noreturn]] void fail(std::string message,Error::Code code=Error::Code::Parse)const {
    throw Error{code,std::string(file)+":"+std::to_string(position)+": "+message};
  }
  void space(){while(position<source.size()&&(source[position]==' '||source[position]=='\t'||source[position]=='\n'||source[position]=='\r'))++position;}
  bool take(char c){space();if(position<source.size()&&source[position]==c){++position;return true;}return false;}
  void require(char c){if(!take(c))fail(std::string("expected '")+c+"'");}
  uint32_t hex() {
    uint32_t out=0;
    for(unsigned n=0;n<4;++n){if(position==source.size())fail("truncated Unicode escape");char c=source[position++];unsigned digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:16;if(digit==16)fail("invalid Unicode escape");out=16*out+digit;}
    return out;
  }
  static void unicode(std::string& out,uint32_t c) {
    if(c<0x80)out+=char(c);
    else if(c<0x800){out+=char(0xc0|(c>>6));out+=char(0x80|(c&63));}
    else if(c<0x10000){out+=char(0xe0|(c>>12));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}
    else {out+=char(0xf0|(c>>18));out+=char(0x80|((c>>12)&63));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}
  }
  std::string string() {
    require('"');std::string out;
    while(position<source.size()) {
      auto c=static_cast<unsigned char>(source[position++]);if(c=='"')return out;if(c<32)fail("unescaped string control character");
      if(c!='\\'){out+=char(c);continue;}
      if(position==source.size())fail("truncated string escape");
      switch(source[position++]) {
        case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;
        case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
        case 'u': {auto u=hex();if(u>=0xd800&&u<=0xdbff){if(position+2>source.size()||source.substr(position,2)!="\\u")fail("missing low surrogate");position+=2;auto low=hex();if(low<0xdc00||low>0xdfff)fail("invalid low surrogate");u=0x10000+((u-0xd800)<<10)+(low-0xdc00);}else if(u>=0xdc00&&u<=0xdfff)fail("unpaired low surrogate");unicode(out,u);break;}
        default:fail("unknown string escape");
      }
    }
    fail("unterminated string");
  }
  Value value(unsigned depth) {
    if(depth>256)fail("JSON nesting limit",Error::Code::ResourceLimit);space();auto begin=position;Value out;
    if(position==source.size())fail("expected value");char c=source[position];
    if(c=='"')out=Value(string());
    else if(c=='{') {
      ++position;Value::Object members;
      if(!take('}'))for(;;){auto key=string();require(':');auto member=value(depth+1);if(!members.emplace(std::move(key),std::move(member)).second)fail("duplicate JSON field");if(take('}'))break;require(',');}
      out=Value(std::move(members));
    }else if(c=='[') {
      ++position;Value::Array items;
      if(!take(']'))for(;;){items.push_back(value(depth+1));if(take(']'))break;require(',');}out=Value(std::move(items));
    }else if(source.substr(position,4)=="true"){position+=4;out=Value(true);}
    else if(source.substr(position,5)=="false"){position+=5;out=Value(false);}
    else if(source.substr(position,4)=="null")fail("null metadata is unsupported",Error::Code::Unsupported);
    else {
      bool negative=c=='-';if(negative)++position;size_t digits=position;
      while(position<source.size()&&source[position]>='0'&&source[position]<='9')++position;
      if(digits==position||(position-digits>1&&source[digits]=='0'))fail("invalid JSON integer");
      if(position<source.size()&&(source[position]=='.'||source[position]=='e'||source[position]=='E'))fail("floating metadata requires a string contract",Error::Code::Unsupported);
      auto text=source.substr(begin,position-begin);
      if(negative){int64_t number;auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),number);if(error!=std::errc{}||end!=text.data()+text.size())fail("integer outside signed 64-bit range");out=Value(number);}
      else {uint64_t number;auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),number);if(error!=std::errc{}||end!=text.data()+text.size())fail("integer outside unsigned 64-bit range");out=Value(number);}
    }
    out.source={std::string(file),begin,1,1};return out;
  }
 public:
  Reader(std::string_view s,std::string_view f):source(s),file(f){}
  Value read(){auto out=value(0);space();if(position!=source.size())fail("trailing JSON input");return out;}
};
std::string quote(std::string_view s) {
  constexpr char hex[]="0123456789abcdef";std::string out="\"";
  for(unsigned char c:s){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}return out+'"';
}
std::string print(const Value& v,unsigned depth) {
  if(depth>256)throw Error{Error::Code::ResourceLimit,"JSON nesting limit"};
  if(auto s=std::get_if<std::string>(&v.data))return quote(*s);
  if(auto a=std::get_if<Value::Array>(&v.data)){std::string out="[";for(size_t k=0;k<a->size();++k){if(k)out+=',';out+=print((*a)[k],depth+1);}return out+']';}
  if(auto o=std::get_if<Value::Object>(&v.data)){std::map<std::string,Value> sorted(o->begin(),o->end());std::string out="{";bool first=true;for(auto& [key,value]:sorted){if(!first)out+=',';first=false;out+=quote(key)+':'+print(value,depth+1);}return out+'}';}
  return v.text();
}
}
Result<Value> parse_json(std::string_view source,std::string_view file){try{return Result<Value>::ok(Reader(source,file).read());}catch(const Error& e){return Result<Value>::err(e);}}
Result<std::string> print_json(const Value& value){try{return Result<std::string>::ok(print(value,0));}catch(const Error& e){return Result<std::string>::err(e);}}
}
