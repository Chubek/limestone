#include "tunah.hpp"
#include <EquinoxNG.hpp>
#include <SExprTk.hpp>
#include <EkippX.hpp>
#include <charconv>
#include <chrono>
#include <limits>
#include <map>
#include <set>

namespace limestone::tunah {
namespace {
using ETerm=equinoxng::Term;
// Check lexical failure cases that SExprTk's permissive data parser accepts.
void lexical_check(std::string_view text) {
  bool quoted=false,escape=false,comment=false;size_t depth=0;
  for(char c:text) {
    if(comment){if(c=='\n')comment=false;continue;}
    if(quoted){if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quoted=false;continue;}
    if(c==';'){comment=true;continue;}
    if(c=='"'){quoted=true;continue;}
    if(c=='('&&++depth>256)throw Error{Error::Code::ResourceLimit,"S-expression nesting limit exceeded"};
    if(c==')'){if(!depth)throw Error{Error::Code::Parse,"unexpected ')'"};--depth;}
  }
  if(quoted||depth)throw Error{Error::Code::Parse,"unterminated S-expression or string"};
}
sexprtk::Cartable parse(std::string_view text) {
  lexical_check(text);
  auto parsed=sexprtk::SExprTk{}.parse(sexprtk::Source::from_string(std::string(text),"<tunah>"));
  if(!parsed.ok())throw Error{Error::Code::Parse,parsed.errors.front()};
  return parsed;
}
Term term(const sexprtk::Cell& c) {
  if(!c.tail.empty())throw Error{Error::Code::Unsupported,"quoted rewrite terms are unsupported"};
  const auto& atom=c.head;
  if(atom.is_int())return {"",{},atom.as_int()};
  if(atom.is_symbol()) {
    auto name=atom.as_string();
    if(!name.empty()&&(std::isdigit(static_cast<unsigned char>(name.front()))||name.front()=='-')) {
      int64_t n=0;auto [p,e]=std::from_chars(name.data(),name.data()+name.size(),n);
      if(e!=std::errc{}||p!=name.data()+name.size())throw Error{Error::Code::Parse,"invalid integer atom: "+name};
    }
    return {name,{},{}};
  }
  if(atom.is_list()) {
    const auto& list=atom.as_list();
    if(list.empty()||!list.front().head.is_symbol())throw Error{Error::Code::Parse,"term requires an operator symbol"};
    Term t{list.front().head.as_string(),{},{}};
    for(size_t i=1;i<list.size();++i)t.arguments.push_back(term(list[i]));
    return t;
  }
  throw Error{Error::Code::Unsupported,"Tunah term adapter supports integers and symbols"};
}
ETerm convert(const Term& t,const std::unordered_map<std::string,size_t>& operators,bool pattern,std::set<std::string>& variables) {
  if(!t.children.empty())throw Error{Error::Code::Unsupported,"flat term IDs require an IL adapter; use structured arguments"};
  if(t.constant) {
    if(!t.arguments.empty())throw Error{Error::Code::InvalidArgument,"constant has child terms"};
    return ETerm::lit(equinoxng::Literal(*t.constant));
  }
  if(t.op.empty())throw Error{Error::Code::InvalidArgument,"empty term operator"};
  if(t.op.front()=='?') {
    if(!pattern||t.op.size()==1||!t.arguments.empty())throw Error{Error::Code::InvalidArgument,"invalid pattern variable"};
    auto name=t.op.substr(1);variables.insert(name);return ETerm::var(name);
  }
  auto signature=operators.find(t.op);
  if((signature==operators.end()&&!t.arguments.empty())||(signature!=operators.end()&&signature->second!=t.arguments.size()))throw Error{Error::Code::InvalidArgument,"unknown operator or wrong arity: "+t.op};
  std::vector<ETerm> children;for(auto& c:t.arguments)children.push_back(convert(c,operators,pattern,variables));
  return ETerm::op(t.op,std::move(children));
}
size_t size(const ETerm& t) { size_t n=1;if(t.is_node())for(auto& c:t.as_node().children)n+=size(c);return n; }
std::string render(const ETerm& t) {
  if(t.is_lit())return equinoxng::literal_to_string(t.as_lit());
  if(t.is_var())return "?"+t.as_var().name;
  const auto& n=t.as_node();if(n.children.empty())return n.op.name;
  std::string s="("+n.op.name;for(auto& c:n.children)s+=" "+render(c);return s+")";
}
}
Result<int> Session::load_rules(std::string_view specification) {
  try {
    ekippx::Context preprocessor;auto text=preprocessor.expand_text(specification,"<tunah>");
    auto parsed=parse(text);std::vector<RewriteRule> compiled;
    std::set<std::string> names;for(auto& r:rules_)names.insert(r.name);
    for(auto& c:parsed.root.cells) {
      if(!c.head.is_list())throw Error{Error::Code::Parse,"expected (rule name pattern replacement)"};
      const auto& list=c.head.as_list();
      if(list.size()!=4||!list[0].head.is_symbol()||list[0].head.as_string()!="rule"||!list[1].head.is_symbol())throw Error{Error::Code::Parse,"expected (rule name pattern replacement); rule clauses need an explicit analysis adapter"};
      RewriteRule rule{list[1].head.as_string(),term(list[2]),term(list[3])};
      if(!names.insert(rule.name).second)throw Error{Error::Code::Conflict,"duplicate rewrite rule: "+rule.name};
      std::set<std::string> lhs,rhs;convert(rule.lhs,operators_,true,lhs);convert(rule.rhs,operators_,true,rhs);
      for(auto& v:rhs)if(!lhs.contains(v))throw Error{Error::Code::InvalidArgument,"unbound replacement variable: ?"+v};
      compiled.push_back(std::move(rule));
    }
    const auto count=compiled.size();rules_.insert(rules_.end(),compiled.begin(),compiled.end());return Result<int>::ok(static_cast<int>(count));
  }catch(const Error& e){return Result<int>::err(e);}catch(const std::exception& e){return Result<int>::err({Error::Code::Parse,e.what()});}
}
Result<SaturationResult> Session::saturate(std::string_view root,Limits limits) const {
  try {
    auto parsed=parse(root);
    if(parsed.root.size()!=1)throw Error{Error::Code::Parse,"expected exactly one input term"};
    std::set<std::string> variables;auto input=convert(term(parsed.root.front()),operators_,false,variables);
    if(!limits.nodes||!limits.classes||size(input)>limits.nodes||size(input)>limits.classes)throw Error{Error::Code::ResourceLimit,"input exceeds e-graph budget"};
    equinoxng::EGraph graph;auto handle=graph.add(input);SaturationResult result;
    const auto start=std::chrono::steady_clock::now();
    auto stopped=[&]{
      return (limits.cancelled&&limits.cancelled())||(limits.time_ms&&std::chrono::steady_clock::now()-start>=std::chrono::milliseconds(limits.time_ms));
    };
    auto rules=rules_;std::sort(rules.begin(),rules.end(),[](auto& a,auto& b){return a.name<b.name;});
    std::set<std::string> names;std::vector<equinoxng::RewriteRule> compiled;
    for(auto& r:rules) {
      if(r.name.empty()||!names.insert(r.name).second)throw Error{Error::Code::InvalidArgument,"empty or duplicate rule name"};
      std::set<std::string> lhs_vars,rhs_vars;auto lhs=convert(r.lhs,operators_,true,lhs_vars),rhs=convert(r.rhs,operators_,true,rhs_vars);
      for(auto& v:rhs_vars)if(!lhs_vars.contains(v))throw Error{Error::Code::InvalidArgument,"unbound replacement variable: ?"+v};
      const size_t growth=size(rhs);const auto name=r.name;
      compiled.emplace_back(name,std::move(lhs),std::move(rhs),[&,growth,name](const equinoxng::Subst& bindings){
        if(stopped()||graph.num_enodes()+growth>limits.nodes||graph.num_classes()+growth>limits.classes){result.limit_reached=true;return false;}
        std::map<std::string,uint32_t> sorted;for(auto&[key,id]:bindings)sorted[key]=graph.find(id).value;
        std::string step=name;for(auto&[key,id]:sorted)step+=" ?"+key+"="+std::to_string(id);
        result.trace.push_back(std::move(step));return true;
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
    graph.rebuild();auto extracted=graph.extract(handle);
    if(extracted.cost==std::numeric_limits<size_t>::max())throw Error{Error::Code::Internal,"e-graph extraction failed"};
    result.expression=render(extracted.term);result.cost=extracted.cost;result.nodes=graph.num_enodes();
    return Result<SaturationResult>::ok(std::move(result));
  }catch(const Error& e){return Result<SaturationResult>::err(e);}catch(const std::exception& e){return Result<SaturationResult>::err({Error::Code::Internal,e.what()});}
}
}
