#include "operand_constraints.hpp"
#include <map>

namespace limestone::metacode {
namespace {
[[noreturn]] void fail(const Value& input,std::string message,Error::Code code=Error::Code::InvalidArgument) {
  if(!input.source.file.empty())message=input.source.file+":"+std::to_string(input.source.line)+":"+std::to_string(input.source.column)+": "+message;
  throw Error{code,std::move(message)};
}
const Value& field(const Value::Object& object,const std::string& name) {
  auto found=object.find(name);if(found==object.end())throw Error{Error::Code::InvalidArgument,"missing operand constraint field: "+name};return found->second;
}
std::string symbol(const Value& value) {
  auto text=std::get_if<std::string>(&value.data);if(!text||text->empty())fail(value,"operand constraint name must be a nonempty string");return *text;
}
}
Result<std::vector<OperandConstraint>> load_operand_constraints(const Value::Object& fields) {
  try {
    if(fields.size()!=1||!fields.contains("constraints"))throw Error{Error::Code::Unsupported,"where contract requires only a constraints array"};
    const auto& input=fields.at("constraints");auto array=std::get_if<Value::Array>(&input.data);if(!array)fail(input,"operand constraints must be an array");
    std::vector<OperandConstraint> constraints;
    for(auto& entry:*array) {
      auto object=std::get_if<Value::Object>(&entry.data);if(!object)fail(entry,"operand constraint must be an object");
      auto kind=symbol(field(*object,"kind"));auto name=std::find(std::begin(operand_predicate_names),std::end(operand_predicate_names),kind);
      if(name==std::end(operand_predicate_names))fail(entry,"unknown operand predicate: "+kind,Error::Code::Unsupported);
      OperandConstraint constraint{static_cast<OperandPredicate>(name-std::begin(operand_predicate_names)),symbol(field(*object,"operand"))};
      if(auto other=object->find("other");other!=object->end())constraint.other=symbol(other->second);
      if(auto value=object->find("value");value!=object->end()) {
        if(auto n=std::get_if<int64_t>(&value->second.data))constraint.value=*n;
        else if(auto n=std::get_if<uint64_t>(&value->second.data);n&&*n<=uint64_t(INT64_MAX))constraint.value=int64_t(*n);
        else fail(value->second,"operand constraint value must be a signed 64-bit integer");
      }
      std::map<std::string,Value> ordered(object->begin(),object->end());
      for(auto& [key,value]:ordered)if(key!="kind"&&key!="operand"&&key!="other"&&key!="value")fail(value,"unknown operand constraint field: "+key,Error::Code::Unsupported);
      std::vector<std::string> bindings{constraint.operand,constraint.other};auto valid=validate_operand_constraints(std::span{&constraint,1},bindings);if(!valid)fail(entry,valid.error().message,valid.error().code);
      constraints.push_back(std::move(constraint));
    }
    return Result<std::vector<OperandConstraint>>::ok(std::move(constraints));
  }catch(const Error& error){return Result<std::vector<OperandConstraint>>::err(error);}
}
Value::Object operand_constraints_metadata(std::span<const OperandConstraint> constraints) {
  Value::Array array;
  for(auto& constraint:constraints) {
    Value::Object entry{{"kind",Value(std::string(predicate_name(constraint.predicate)))},{"operand",Value(constraint.operand)}};
    if(!constraint.other.empty())entry["other"]=Value(constraint.other);
    if(constraint.value)entry["value"]=Value(*constraint.value);
    array.emplace_back(std::move(entry));
  }
  return {{"constraints",Value(std::move(array))}};
}
}
