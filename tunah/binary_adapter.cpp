#include "binary_adapter.hpp"
#include <map>

namespace limestone::tunah {
namespace {
void field(std::string& identity,std::string_view value){identity+=std::to_string(value.size())+":";identity.append(value);}
}
Result<bin2bin::SemanticTransform> binary_transform(const Session& session,std::string context,const BinaryAdapterOptions& options) {
  if(context.empty())return Result<bin2bin::SemanticTransform>::err({Error::Code::InvalidArgument,"binary Tunah adapter needs a semantic/analysis context identity"});
  if(!options.legality)return Result<bin2bin::SemanticTransform>::err({Error::Code::Unsupported,"binary Tunah adapter needs a target legality analysis"});
  // Construct a bounded, deterministic identity rather than hashing addresses or
  // hiding rule changes behind a caller's version string. Source locations do
  // not alter generated semantics and are retained in the owning session snapshot.
  bin2bin::SemanticTransform transform;transform.identity="tunah.binary:1:";field(transform.identity,context);
  std::map<std::string,size_t> operators(session.operators().begin(),session.operators().end());field(transform.identity,std::to_string(operators.size()));
  for(auto& [name,arity]:operators){field(transform.identity,name);field(transform.identity,std::to_string(arity));}
  auto rules=session.rules();std::sort(rules.begin(),rules.end(),[](auto& a,auto& b){return a.name<b.name;});field(transform.identity,std::to_string(rules.size()));
  for(auto& rule:rules){field(transform.identity,rule.name);field(transform.identity,format_term(rule.lhs));field(transform.identity,format_term(rule.rhs));field(transform.identity,std::to_string(rule.conditions.size()));for(auto& condition:rule.conditions){field(transform.identity,condition.predicate);field(transform.identity,std::to_string(condition.arguments.size()));for(auto& term:condition.arguments)field(transform.identity,format_term(term));}}
  std::map<std::string,size_t> costs(options.costs.operators.begin(),options.costs.operators.end());field(transform.identity,std::to_string(options.costs.literal));field(transform.identity,std::to_string(costs.size()));for(auto [name,cost]:costs){field(transform.identity,name);field(transform.identity,std::to_string(cost));}
  for(auto n:{options.limits.iterations,options.limits.nodes,options.limits.classes})field(transform.identity,std::to_string(n));field(transform.identity,std::to_string(options.limits.time_ms));
  transform.cacheable=!options.limits.time_ms&&!options.limits.cancelled;
  transform.apply=[snapshot=std::make_shared<const Session>(session),options](const bin2bin::LiftedInstruction& instruction)->Result<std::string> {
    if(instruction.status!=bin2bin::Status::Supported)return Result<std::string>::err({Error::Code::Unsupported,"cannot optimize unsupported lifted instruction"});
    auto source="<binary:"+std::to_string(instruction.address)+">";auto term=parse_term(instruction.semantics,source);if(!term)return Result<std::string>::err(term.error());
    auto ingress=options.legality(instruction,term.value());if(!ingress)return Result<std::string>::err(ingress.error());if(!ingress.value())return Result<std::string>::err({Error::Code::Conflict,source+": illegal input semantic term"});
    auto result=snapshot->saturate(term.value(),options.limits,options.costs);if(!result)return Result<std::string>::err(result.error());
    auto checked=options.legality(instruction,result.value().term);if(!checked)return Result<std::string>::err(checked.error());if(!checked.value())return Result<std::string>::err({Error::Code::Conflict,source+": illegal extracted semantic term"});
    return Result<std::string>::ok(result.value().expression);
  };
  return Result<bin2bin::SemanticTransform>::ok(std::move(transform));
}
}
