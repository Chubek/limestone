#include "native.hpp"
#include <bit>
#include <set>

namespace limestone::traceml {
struct NativeProgramStorage {RuntimeProgram program;std::string identity;NativeArtifact artifact;};
namespace {
template<class T,class Function> Result<T> checked(Function&& function){
  try{return function();}catch(const Error& error){return Result<T>::err(error);}catch(const std::bad_alloc&){return Result<T>::err({Error::Code::ResourceLimit,"TraceLambda native allocation failed"});}catch(const std::exception& error){return Result<T>::err({Error::Code::Internal,error.what()});}catch(...){return Result<T>::err({Error::Code::Internal,"TraceLambda native adapter exception"});}
}
Result<int> validate_guard(const NativeGuard& guard){
  if(!guard.snapshot.valid()||guard.condition.kind<NativeLocation::Kind::Register||guard.condition.kind>NativeLocation::Kind::Constant||bool(guard.recover)!=bool(guard.prove))return Result<int>::err({Error::Code::InvalidArgument,"native guard needs a valid snapshot/location and complete recovery proof"});
  return Result<int>::ok(0);
}
}
Result<int64_t> recover_integer(const NativeState& state,const NativeLocation& location){
  if(location.kind==NativeLocation::Kind::Constant)return Result<int64_t>::ok(location.constant);
  if(location.kind==NativeLocation::Kind::Register){auto value=state.registers.find(location.reg);if(value==state.registers.end()||!value->second.integer())return Result<int64_t>::err({Error::Code::Conflict,"native recovery register is missing or noninteger"});return Result<int64_t>::ok(*value->second.integer());}
  if(location.kind!=NativeLocation::Kind::Stack)return Result<int64_t>::err({Error::Code::InvalidArgument,"unknown native recovery location"});
  if(state.byte_order!="little"&&state.byte_order!="big")return Result<int64_t>::err({Error::Code::InvalidArgument,"native stack recovery needs explicit byte order"});
  if(location.offset>state.stack.size()||8>state.stack.size()-location.offset)return Result<int64_t>::err({Error::Code::Conflict,"native recovery stack slot is outside safepoint image"});
  uint64_t value=0;for(unsigned k=0;k<8;++k)value|=uint64_t(state.stack[size_t(location.offset)+k])<<(8*(state.byte_order=="little"?k:7-k));return Result<int64_t>::ok(std::bit_cast<int64_t>(value));
}
Result<RuntimeResult> deoptimize(const NativeGuard& input,const NativeState& input_state,const ExecutionOptions& input_options){
  return checked<RuntimeResult>([&]()->Result<RuntimeResult>{
    auto guard=input;auto state=input_state;auto options=input_options;auto valid=validate_guard(guard);if(!valid)return Result<RuntimeResult>::err(valid.error());
    auto condition=recover_integer(state,guard.condition);if(!condition)return Result<RuntimeResult>::err(condition.error());auto snapshot=guard.snapshot;
    if(guard.recover){auto recovered=guard.recover(snapshot,state);if(!recovered)return Result<RuntimeResult>::err(recovered.error());if(!recovered.value().valid())return Result<RuntimeResult>::err({Error::Code::Conflict,"native recovery returned an empty continuation"});valid=guard.prove(snapshot,recovered.value(),state);if(!valid)return Result<RuntimeResult>::err(valid.error());snapshot=std::move(recovered.value());}
    return resume_guard(snapshot,condition.value(),options);
  });
}
Result<NativeProgram> lower_native(const RuntimeProgram& input,const NativeRuntimeAdapter& input_adapter){
  return checked<NativeProgram>([&]()->Result<NativeProgram>{
    auto program=input;auto adapter=input_adapter;
    if(!program.valid()||adapter.identity.empty()||!adapter.lower||!adapter.prove)return Result<NativeProgram>::err({Error::Code::InvalidArgument,"native runtime lowering needs an owning program, adapter identity, lowering and semantic proof"});
    auto artifact=adapter.lower(program);if(!artifact)return Result<NativeProgram>::err(artifact.error());auto& result=artifact.value();
    if(!result.owner||!result.invoke||result.bytes.empty()||result.machine_ir.empty())return Result<NativeProgram>::err({Error::Code::Conflict,"native lowering returned incomplete executable/IR ownership"});
    if(result.bytes.size()>adapter.byte_limit||result.guards.size()>adapter.guard_limit)return Result<NativeProgram>::err({Error::Code::ResourceLimit,"native runtime lowering byte/guard limit"});
    std::set<uint32_t> ids;for(auto& guard:result.guards){if(!ids.insert(guard.id).second)return Result<NativeProgram>::err({Error::Code::Conflict,"duplicate native guard identity"});auto valid=validate_guard(guard);if(!valid)return Result<NativeProgram>::err(valid.error());}
    auto valid=adapter.prove(program,result);if(!valid)return Result<NativeProgram>::err(valid.error());auto storage=std::make_shared<NativeProgramStorage>();storage->program=std::move(program);storage->identity=std::move(adapter.identity);storage->artifact=std::move(result);return Result<NativeProgram>::ok(NativeProgram(std::move(storage)));
  });
}
Result<RuntimeResult> run_native(const NativeProgram& program,std::span<const RuntimeValue> input,const ExecutionOptions& input_options){
  return checked<RuntimeResult>([&]()->Result<RuntimeResult>{
    auto active=program.storage_;if(!active)return Result<RuntimeResult>::err({Error::Code::InvalidArgument,"empty native runtime program"});std::vector<RuntimeValue> arguments(input.begin(),input.end());auto options=input_options;
    for(auto& value:arguments)if(!value.integer()&&!value.callable())return Result<RuntimeResult>::err({Error::Code::InvalidArgument,"empty native runtime argument"});
    if(options.cancelled&&options.cancelled())return Result<RuntimeResult>::err({Error::Code::Interrupted,"native runtime execution cancelled"});
    auto result=active->artifact.invoke(arguments,options);if(!result)return result;
    if(!result.value().value.integer()&&!result.value().value.callable())return Result<RuntimeResult>::err({Error::Code::Conflict,"native runtime returned an empty value"});
    if(result.value().value.integer()&&*result.value().value.integer()!=result.value().execution.value)return Result<RuntimeResult>::err({Error::Code::Conflict,"native runtime result disagrees with execution record"});
    return result;
  });
}
std::string_view NativeProgram::identity() const {return storage_?std::string_view(storage_->identity):std::string_view{};}
std::string_view NativeProgram::machine_ir() const {return storage_?std::string_view(storage_->artifact.machine_ir):std::string_view{};}
std::span<const uint8_t> NativeProgram::bytes() const {return storage_?std::span<const uint8_t>(storage_->artifact.bytes):std::span<const uint8_t>{};}
}
