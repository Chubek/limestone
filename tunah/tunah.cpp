#include "detail.hpp"
#include <EquinoxNG.hpp>
#include <chrono>
#include <limits>
#include <map>

namespace limestone::tunah::detail {
// Equinox-NG accepts a host-defined cost type. Saturating arithmetic prevents
// its additive extractor from wrapping a large cost into a cheap alternative.
struct CheckedCost {
  size_t value=0;
  explicit constexpr operator size_t() const { return value; }
  constexpr bool operator==(const CheckedCost&) const = default;
  constexpr bool operator<(const CheckedCost& other) const { return value<other.value; }
  CheckedCost& operator+=(CheckedCost other) {
    const auto maximum=std::numeric_limits<size_t>::max();
    value=other.value>maximum-value?maximum:value+other.value;
    return *this;
  }
};
}
template<> struct std::numeric_limits<limestone::tunah::detail::CheckedCost> : std::numeric_limits<size_t> {
  static constexpr limestone::tunah::detail::CheckedCost max() noexcept { return {std::numeric_limits<size_t>::max()}; }
};

namespace limestone::tunah {
namespace {
using ETerm=equinoxng::Term;
ETerm convert(const Term& term) {
  if(term.constant) return ETerm::lit(equinoxng::Literal(*term.constant));
  if(term.op.front()=='?') return ETerm::var(term.op.substr(1));
  std::vector<ETerm> children;
  children.reserve(term.arguments.size());
  for(const auto& child:term.arguments) children.push_back(convert(child));
  return ETerm::op(term.op,std::move(children));
}
Term reconstruct(const ETerm& term) {
  Term result;
  if(term.is_lit()) result.constant=std::get<int64_t>(term.as_lit().value);
  else if(term.is_var()) result.op="?"+term.as_var().name;
  else {
    result.op=term.as_node().op.name;
    result.application=!term.as_node().children.empty();
    result.arguments.reserve(term.as_node().children.size());
    for(const auto& child:term.as_node().children) result.arguments.push_back(reconstruct(child));
  }
  return result;
}
size_t size(const ETerm& term) {
  size_t count=1;
  if(term.is_node()) for(const auto& child:term.as_node().children) count+=size(child);
  return count;
}
struct ExtractionCost {
  using cost_type=detail::CheckedCost;
  const CostModel& model;
  cost_type literal_cost(const equinoxng::Literal&) const { return {model.literal}; }
  cost_type symbol_cost(const equinoxng::Symbol& symbol, size_t arity) const {
    auto cost=model.operators.find(symbol.name);
    return {cost==model.operators.end()?1+arity:cost->second};
  }
};
}
struct Session::PreparedRules {
  struct Rule { size_t index; ETerm lhs, rhs; size_t growth; std::string trace_name; };
  std::vector<Rule> rules;
};
std::shared_ptr<const Session::PreparedRules> Session::prepare_rules(const std::vector<RewriteRule>& rules,
  const std::unordered_map<std::string,size_t>& operators) const {
  auto prepared=std::make_shared<PreparedRules>();
  std::set<std::string> names;
  for(size_t index=0;index<rules.size();++index) {
    const auto& rule=rules[index];
    if(!names.insert(rule.name).second) throw detail::diagnostic(Error::Code::InvalidArgument,"duplicate rule name: "+rule.name,rule.location);
    validate_rule(rule,operators);
    auto lhs=convert(rule.lhs),rhs=convert(rule.rhs);
    auto growth=size(lhs)+size(rhs);
    auto trace_name=rule.name;
    if(!rule.location.file.empty()&&rule.location.file!="<tunah>")
      trace_name+=" @"+rule.location.file+(rule.location.expanded?" [expanded]":"")+":"+std::to_string(rule.location.line)+":"+std::to_string(rule.location.column);
    prepared->rules.push_back({index,std::move(lhs),std::move(rhs),growth,std::move(trace_name)});
  }
  std::sort(prepared->rules.begin(),prepared->rules.end(),[&](const auto& a,const auto& b){return rules[a.index].name<rules[b.index].name;});
  return prepared;
}
Result<SaturationResult> Session::saturate(std::string_view root, Limits limits, const CostModel& costs) const {
  auto parsed=parse_term(root);
  if(!parsed) return Result<SaturationResult>::err(parsed.error());
  return saturate(parsed.value(),std::move(limits),costs);
}
Result<SaturationResult> Session::saturate(const Term& root, Limits limits, const CostModel& costs) const {
  try {
    std::set<std::string> variables;
    detail::validate_term(root,operators_,false,variables);
    if(costs.literal==std::numeric_limits<size_t>::max()) throw Error{Error::Code::InvalidArgument,"literal cost is reserved for infinity"};
    std::map<std::string,size_t> ordered_costs(costs.operators.begin(),costs.operators.end());
    for(const auto& [op,cost]:ordered_costs) {
      if(!operators_.contains(op)) throw Error{Error::Code::InvalidArgument,"cost for undeclared operator: "+op};
      if(cost==std::numeric_limits<size_t>::max()) throw Error{Error::Code::InvalidArgument,"operator cost is reserved for infinity: "+op};
    }
    auto input=convert(root);
    const auto input_size=size(input);
    if(!limits.nodes||!limits.classes||input_size>limits.nodes||input_size>limits.classes)throw Error{Error::Code::ResourceLimit,"input exceeds e-graph budget"};
    equinoxng::EGraph graph;auto handle=graph.add(input);SaturationResult result;
    const auto start=std::chrono::steady_clock::now();
    auto stopped=[&]{
      return (limits.cancelled&&limits.cancelled())||(limits.time_ms&&
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count())>=limits.time_ms);
    };
    auto prepared=prepared_?prepared_:prepare_rules(rules_,operators_);
    std::vector<equinoxng::RewriteRule> compiled;
    compiled.reserve(prepared->rules.size());
    const CostModel analysis_costs;
    for(const auto& pattern:prepared->rules) {
      const auto* origin=&rules_[pattern.index];
      const auto* trace_name=&pattern.trace_name;
      const size_t growth=pattern.growth;
      compiled.emplace_back(origin->name,pattern.lhs,pattern.rhs,[&,growth,origin,trace_name](const equinoxng::Subst& bindings){
        const auto& rule=*origin;
        if(stopped()) { result.limit_reached=true; return false; }
        std::map<std::string,Term> representatives;
        for(const auto& condition:rule.conditions) {
          std::vector<Term> arguments;
          for(const auto& argument:condition.arguments) {
            if(argument.constant) { arguments.push_back(argument); continue; }
            auto variable=argument.op.substr(1);
            auto found=representatives.find(variable);
            if(found==representatives.end()) {
              auto extracted=graph.extract(bindings.at(variable),ExtractionCost{analysis_costs});
              if(extracted.cost==std::numeric_limits<size_t>::max()) throw Error{Error::Code::Internal,"predicate binding extraction failed"};
              found=representatives.emplace(variable,reconstruct(extracted.term)).first;
            }
            arguments.push_back(found->second);
          }
          auto allowed=predicates_.at(condition.predicate).evaluate(arguments);
          if(!allowed) throw detail::diagnostic(allowed.error().code,"predicate "+condition.predicate+": "+allowed.error().message,rule.location);
          if(!allowed.value()) return false;
        }
        // A conservative reservation covers both sides instantiated by the
        // vendor engine. Subtraction avoids overflow in the budget check.
        if(stopped()||graph.num_enodes()>limits.nodes||growth>limits.nodes-graph.num_enodes()||
           graph.num_classes()>limits.classes||growth>limits.classes-graph.num_classes()) {
          result.limit_reached=true; return false;
        }
        if(limits.trace) {
          std::map<std::string,uint32_t> sorted;
          for(const auto& [key,id]:bindings) sorted[key]=graph.find(id).value;
          std::string step=*trace_name;
          for(const auto& [key,id]:sorted) step+=" ?"+key+"="+std::to_string(id);
          result.trace.push_back(std::move(step));
        }
        return true;
      });
    }
    for(size_t iteration=0;iteration<limits.iterations;++iteration) {
      if(stopped()){result.limit_reached=true;break;}
      auto before=graph.num_enodes();size_t applied=0;
      for(auto& r:compiled){applied+=graph.apply_rule(r);graph.rebuild();if(result.limit_reached)break;}
      result.rewrites+=applied;++result.iterations;
      if(result.limit_reached)break;
      if(!applied&&before==graph.num_enodes()){result.saturated=true;break;}
    }
    if(!result.saturated&&result.iterations==limits.iterations)result.limit_reached=true;
    graph.rebuild();auto extracted=graph.extract(handle,ExtractionCost{costs});
    if(extracted.cost==std::numeric_limits<size_t>::max())throw Error{Error::Code::ResourceLimit,"extraction cost exceeds the representable range"};
    result.term=reconstruct(extracted.term); result.term.location=root.location;
    result.expression=format_term(result.term);result.cost=extracted.cost;result.nodes=graph.num_enodes();result.classes=graph.num_classes();
    return Result<SaturationResult>::ok(std::move(result));
  }catch(const Error& e){return Result<SaturationResult>::err(e);}catch(const std::exception& e){return Result<SaturationResult>::err({Error::Code::Internal,e.what()});}
}
}
