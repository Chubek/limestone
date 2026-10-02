#include "unisel.hpp"
#include <SatieDPLL.hpp>
#include <bit>
#include <limits>
#include <map>
#include <set>

namespace limestone::unisel {
Result<int> validate(const Program& p,const std::vector<Pattern>& patterns) {
  std::map<NodeId,const Node*> nodes;
  for(auto& n:p.nodes)if(n.op.empty()||!nodes.emplace(n.id,&n).second)return Result<int>::err({Error::Code::InvalidArgument,"empty operation or duplicate source node"});
  std::map<NodeId,size_t> indegree;std::map<NodeId,std::vector<NodeId>> out;
  for(auto& n:p.nodes) {
    indegree[n.id]=n.inputs.size();
    for(auto id:n.inputs) {
      if(!nodes.contains(id))return Result<int>::err({Error::Code::InvalidArgument,"unknown input to node "+std::to_string(n.id)});
      out[id].push_back(n.id);
    }
    if(!n.required&&(!n.inputs.empty()||n.side_effect))return Result<int>::err({Error::Code::InvalidArgument,"external values must be effect-free leaves"});
  }
  for(auto& d:p.dependencies) {
    if(!nodes.contains(d.producer)||!nodes.contains(d.consumer))return Result<int>::err({Error::Code::InvalidArgument,"unknown semantic dependency endpoint"});
    if(d.distance)return Result<int>::err({Error::Code::Unsupported,"loop-carried selection dependencies require a CFG adapter"});
    ++indegree[d.consumer];out[d.producer].push_back(d.consumer);
  }
  std::set<NodeId> ready;for(auto [id,count]:indegree)if(!count)ready.insert(id);
  size_t visited=0;
  while(!ready.empty()){auto id=*ready.begin();ready.erase(ready.begin());++visited;for(auto child:out[id])if(--indegree[child]==0)ready.insert(child);}
  if(visited!=nodes.size())return Result<int>::err({Error::Code::Conflict,"cyclic source dependencies"});
  for(auto id:p.outputs)if(!nodes.contains(id))return Result<int>::err({Error::Code::InvalidArgument,"unknown program output"});
  std::unordered_set<PatternId> ids;
  std::function<bool(const PatternTree&)> valid_tree=[&](const PatternTree& tree){
    if(tree.immediate&&tree.immediate->first>tree.immediate->second)return false;
    if(tree.op.empty())return tree.inputs.empty();
    return std::all_of(tree.inputs.begin(),tree.inputs.end(),valid_tree);
  };
  for(auto& pat:patterns) {
    if(!ids.insert(pat.id).second||pat.cost<0||pat.instruction.empty()||pat.root_op.empty())return Result<int>::err({Error::Code::InvalidArgument,"invalid instruction pattern"});
    if(pat.tree&&(!valid_tree(*pat.tree)||pat.tree->op!=pat.root_op))return Result<int>::err({Error::Code::InvalidArgument,"invalid structured pattern"});
  }
  return Result<int>::ok(0);
}
std::vector<Candidate> match(const Program& p,const std::vector<Pattern>& patterns) {
  if(!validate(p,patterns))return {};
  std::map<NodeId,const Node*> nodes;std::map<NodeId,std::vector<NodeId>> users;
  for(auto& n:p.nodes){nodes[n.id]=&n;for(auto id:n.inputs)users[id].push_back(n.id);}
  std::vector<Candidate> candidates;
  for(auto& n:p.nodes)if(n.required)for(auto& pat:patterns)if(n.op==pat.root_op) {
     Candidate c{pat.id,n.id,{},pat.cost,pat.name,{}, n.produces_value?std::vector<NodeId>{n.id}:std::vector<NodeId>{}};
    std::map<std::string,NodeId> bindings;bool effects=false;
    std::function<bool(NodeId,const PatternTree&)> visit=[&](NodeId id,const PatternTree& tree) {
       const auto& node=*nodes.at(id);
      if(node.block!=n.block)return false;
      if(!tree.type.empty()&&tree.type!=node.type)return false;
      if(tree.immediate&&(!node.constant||*node.constant<tree.immediate->first||*node.constant>tree.immediate->second))return false;
      if(!tree.binding.empty()) {
        auto [it,added]=bindings.emplace(tree.binding,id);if(!added&&it->second!=id)return false;
      }
       if(tree.op.empty()){if(!node.produces_value)return false;c.inputs.push_back(id);return true;}
      if(tree.op!=node.op||tree.inputs.size()!=node.inputs.size()||!node.required)return false;
      if(std::find(c.covered.begin(),c.covered.end(),id)!=c.covered.end())return false;
      c.covered.push_back(id);effects|=node.side_effect;
      for(size_t i=0;i<node.inputs.size();++i)if(!visit(node.inputs[i],tree.inputs[i]))return false;
      return true;
    };
    PatternTree tree;
    if(pat.tree)tree=*pat.tree;
    else { tree.op=pat.root_op;for(auto& operand:pat.operands)tree.inputs.push_back(PatternTree{}); }
    if(!visit(n.id,tree)||(effects&&!pat.supports_side_effects))continue;
    std::sort(c.covered.begin(),c.covered.end());
    bool escapes=false;
    for(auto id:c.covered)if(id!=n.id) {
      for(auto user:users[id])if(!std::binary_search(c.covered.begin(),c.covered.end(),user))escapes=true;
      if(std::find(p.outputs.begin(),p.outputs.end(),id)!=p.outputs.end())escapes=true;
      // Preserve effect placement: explicit ordering may not be hidden by fusion.
      for(auto& d:p.dependencies)if(d.producer==id||d.consumer==id)escapes=true;
    }
    if(escapes)continue;
    candidates.push_back(std::move(c));
  }
  std::sort(candidates.begin(),candidates.end(),[](auto& a,auto& b){return std::tie(a.root,a.cost,a.pattern)<std::tie(b.root,b.cost,b.pattern);});
  return candidates;
}
Result<ConstraintModel> build_model(const Program& p,const std::vector<Pattern>& patterns) {
  auto valid=validate(p,patterns);if(!valid)return Result<ConstraintModel>::err(valid.error());
  ConstraintModel m{match(p,patterns),{}};
  if(m.candidates.size()>static_cast<size_t>(std::numeric_limits<int32_t>::max()/128))return Result<ConstraintModel>::err({Error::Code::ResourceLimit,"too many candidates"});
  for(auto& n:p.nodes)if(n.required) {
    std::vector<int32_t> covering;
    for(size_t i=0;i<m.candidates.size();++i)if(std::binary_search(m.candidates[i].covered.begin(),m.candidates[i].covered.end(),n.id))covering.push_back(static_cast<int32_t>(i+1));
    // Exactly one implementation prevents both missing coverage and overlap.
    m.clauses.push_back(covering);
    for(size_t i=0;i<covering.size();++i)for(size_t j=i+1;j<covering.size();++j)m.clauses.push_back({-covering[i],-covering[j]});
  }
  return Result<ConstraintModel>::ok(std::move(m));
}
namespace {
struct Encoder {
  std::vector<std::vector<int32_t>> clauses;
  int32_t next;
  int32_t variable(){return ++next;}
  int32_t xor_gate(int32_t a,int32_t b) {
    auto c=variable();clauses.insert(clauses.end(),{{a,b,-c},{a,-b,c},{-a,b,c},{-a,-b,-c}});return c;
  }
  int32_t carry_gate(int32_t a,int32_t b,int32_t c) {
    auto d=variable();clauses.insert(clauses.end(),{{-a,-b,d},{-a,-c,d},{-b,-c,d},{a,b,-d},{a,c,-d},{b,c,-d}});return d;
  }
  std::vector<int32_t> cost(const std::vector<Candidate>& candidates,uint64_t total) {
    auto zero=variable();clauses.push_back({-zero});
    size_t width=std::max<unsigned>(1,std::bit_width(total));std::vector<int32_t> bits(width,zero);
    for(size_t i=0;i<candidates.size();++i) {
      if(!candidates[i].cost)continue;
      int32_t carry=zero;
      for(size_t k=0;k<width;++k) {
        int32_t b=(uint64_t(candidates[i].cost)&(uint64_t(1)<<k))?static_cast<int32_t>(i+1):zero;
        auto sum=xor_gate(xor_gate(bits[k],b),carry);carry=carry_gate(bits[k],b,carry);bits[k]=sum;
      }
      clauses.push_back({-carry});
    }
    return bits;
  }
  std::vector<std::vector<int32_t>> bounded(const std::vector<int32_t>& bits,uint64_t bound) const {
    auto cnf=clauses;
    for(size_t k=0;k<bits.size();++k)if(!(bound&(uint64_t(1)<<k))) {
      std::vector<int32_t> clause{-bits[k]};
      for(size_t j=k+1;j<bits.size();++j)clause.push_back(bound&(uint64_t(1)<<j)?-bits[j]:bits[j]);
      cnf.push_back(std::move(clause));
    }
    return cnf;
  }
};
bool satisfiable(const std::vector<std::vector<int32_t>>& clauses) {
  return satie::solve_dpll(satie::CNF(clauses)).status==satie::SolveStatus::SAT;
}
}
Result<Selection> solve(const Program& p,const std::vector<Pattern>& patterns) {
  auto model=build_model(p,patterns);if(!model)return Result<Selection>::err(model.error());
  auto& m=model.value();uint64_t total=0;for(auto& c:m.candidates)total+=static_cast<uint32_t>(c.cost);
  Encoder e{m.clauses,static_cast<int32_t>(m.candidates.size())};auto bits=e.cost(m.candidates,total);
  if(!satisfiable(e.clauses))return Result<Selection>::err({Error::Code::Unsatisfiable,"no exact instruction covering satisfies the selection model"});
  uint64_t low=0,high=total;
  while(low<high){auto mid=low+(high-low)/2;if(satisfiable(e.bounded(bits,mid)))high=mid;else low=mid+1;}
  if(low>std::numeric_limits<int>::max())return Result<Selection>::err({Error::Code::ResourceLimit,"selection cost overflow"});
  auto cnf=e.bounded(bits,low);Selection s;
  // Fix the optimum lexicographically; vendor decision order is not observable.
  for(size_t i=0;i<m.candidates.size();++i) {
    auto lit=static_cast<int32_t>(i+1);cnf.push_back({lit});
    if(satisfiable(cnf)){s.selected.push_back(m.candidates[i]);s.cost+=m.candidates[i].cost;}
    else cnf.back()[0]=-lit;
  }
  auto emitted=emit_scheduler(p,patterns,s);if(!emitted)return Result<Selection>::err(emitted.error());
  return Result<Selection>::ok(std::move(s));
}
Result<Selection> solve_greedy(const Program& p,const std::vector<Pattern>& patterns) {
  auto valid=validate(p,patterns);if(!valid)return Result<Selection>::err(valid.error());
  auto candidates=match(p,patterns);std::sort(candidates.begin(),candidates.end(),[](auto& a,auto& b){return std::tie(a.cost,a.pattern,a.root)<std::tie(b.cost,b.pattern,b.root);});
  Selection s;std::unordered_set<NodeId> covered;int64_t cost=0;
  for(auto& c:candidates)if(std::none_of(c.covered.begin(),c.covered.end(),[&](auto id){return covered.contains(id);})) {
    s.selected.push_back(c);cost+=c.cost;covered.insert(c.covered.begin(),c.covered.end());
  }
  if(covered.size()!=static_cast<size_t>(std::count_if(p.nodes.begin(),p.nodes.end(),[](auto& n){return n.required;})))return Result<Selection>::err({Error::Code::Unsatisfiable,"greedy selection cannot cover all required operations"});
  if(cost>std::numeric_limits<int>::max())return Result<Selection>::err({Error::Code::ResourceLimit,"selection cost overflow"});
  s.cost=static_cast<int>(cost);auto emitted=emit_scheduler(p,patterns,s);if(!emitted)return Result<Selection>::err(emitted.error());
  return Result<Selection>::ok(std::move(s));
}
Result<schedrow::Region> emit_scheduler(const Program& p,const std::vector<Pattern>& patterns,const Selection& selection) {
  auto valid=validate(p,patterns);if(!valid)return Result<schedrow::Region>::err(valid.error());
  auto candidates=match(p,patterns);std::map<NodeId,NodeId> owner;std::map<PatternId,const Pattern*> pats;
  for(auto& pat:patterns)pats[pat.id]=&pat;
  int64_t cost=0;
  for(auto& c:selection.selected) {
    auto found=std::find_if(candidates.begin(),candidates.end(),[&](auto& x){return x.root==c.root&&x.pattern==c.pattern&&x.covered==c.covered&&x.inputs==c.inputs&&x.outputs==c.outputs&&x.cost==c.cost;});
    if(found==candidates.end())return Result<schedrow::Region>::err({Error::Code::InvalidArgument,"selection contains an invalid candidate"});
    for(auto id:c.covered)if(!owner.emplace(id,c.root).second)return Result<schedrow::Region>::err({Error::Code::Conflict,"overlapping selected candidates"});
    cost+=c.cost;
  }
  for(auto& n:p.nodes)if(n.required&&!owner.contains(n.id))return Result<schedrow::Region>::err({Error::Code::Conflict,"uncovered source operation"});
  if(cost!=selection.cost)return Result<schedrow::Region>::err({Error::Code::Conflict,"selection cost mismatch"});
  schedrow::Region r{"unisel"};std::set<std::tuple<NodeId,NodeId,schedrow::DepKind,uint32_t>> edges;
  auto selected=selection.selected;
  std::sort(selected.begin(),selected.end(),[](auto& a,auto& b){return a.root<b.root;});
  for(auto& c:selected) {
    schedrow::Instruction i{};i.id=c.root;i.opcode=pats.at(c.pattern)->instruction;i.defs=c.outputs;i.uses=c.inputs;
     i.origin="pattern "+std::to_string(c.pattern)+", root "+std::to_string(c.root);
     for(auto id:c.covered) {
       const auto& node=*std::find_if(p.nodes.begin(),p.nodes.end(),[&](auto& n){return n.id==id;});
       if(node.constant)i.immediates.emplace_back(id,*node.constant);
       if(node.side_effect)i.speculative=false;
     }
    r.instructions.push_back(std::move(i));
    for(auto input:c.inputs)if(owner.contains(input)&&owner.at(input)!=c.root)edges.emplace(owner.at(input),c.root,schedrow::DepKind::True,0);
  }
  for(auto& d:p.dependencies)if(owner.contains(d.producer)&&owner.contains(d.consumer)&&owner.at(d.producer)!=owner.at(d.consumer))edges.emplace(owner.at(d.producer),owner.at(d.consumer),d.kind,d.latency);
  for(auto [a,b,kind,latency]:edges)r.deps.push_back({a,b,kind,latency});
  // Detect dependency cycles before exposing a selection to a scheduler.
  std::map<NodeId,size_t> indegree;std::map<NodeId,std::vector<NodeId>> out;
  for(auto& i:r.instructions)indegree[i.id]=0;
  for(auto& d:r.deps){++indegree[d.consumer];out[d.producer].push_back(d.consumer);}
  std::set<NodeId> ready;for(auto [id,n]:indegree)if(!n)ready.insert(id);
  std::vector<schedrow::Instruction> ordered;
  while(!ready.empty()){
    auto id=*ready.begin();ready.erase(ready.begin());
    ordered.push_back(*std::find_if(r.instructions.begin(),r.instructions.end(),[&](auto& i){return i.id==id;}));
    for(auto child:out[id])if(--indegree[child]==0)ready.insert(child);
  }
  if(ordered.size()!=r.instructions.size())return Result<schedrow::Region>::err({Error::Code::Conflict,"selected instruction dependencies are cyclic"});
  r.instructions=std::move(ordered);return Result<schedrow::Region>::ok(std::move(r));
}
}
