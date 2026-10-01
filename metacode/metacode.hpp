#pragma once
#include "../limestone/foundation.hpp"
namespace limestone::metacode {
struct Value {
  using Object=std::unordered_map<std::string,Value>;
  using Array=std::vector<Value>;
  std::variant<std::string,int64_t,bool,Object,Array> data;
  Value() : data(std::string{}) {}
  explicit Value(std::string s):data(std::move(s)){}
  explicit Value(int64_t n):data(n){}
  explicit Value(bool b):data(b){}
  explicit Value(Object o):data(std::move(o)){}
  explicit Value(Array a):data(std::move(a)){}
  std::string text() const;
};
struct Register { std::string name, klass; uint32_t width=0, number=0; };
struct Operation { std::string name, semantics; Value::Object fields; };
struct Architecture {
  std::string name, family, model, version;
  std::unordered_map<std::string,Value> fields;
  std::vector<Register> registers;
  std::vector<Operation> operations;
};
Result<Architecture> parse_isa(std::string_view text);
Result<Architecture> load_isa_file(const std::string& path);
std::string dump(const Architecture&);
}
