#include "limeburg.hpp"
#include <limits>
#include <map>
#include <set>

namespace limestone::limeburg {
namespace {
bool legal_immediate(const Node& n,const std::optional<std::pair<int64_t,int64_t>>& range) {
  return !range||(n.has_imm&&n.imm>=range->first&&n.imm<=range->second);
}
}
Result<int> validate(const RuleSet& rs) {
  std::unordered_set<uint32_t> nts;std::unordered_set<RuleId> ids;
  for(auto&[name,id]:rs.nonterminals)if(name.empty()||!nts.insert(id).second)return Result<int>::err({Error::Code::InvalidArgument,"invalid nonterminal identity"});
  std::function<bool(const Pattern&)> valid_pattern=[&](const Pattern& p) {
    if(p.immediate&&p.immediate->first>p.immediate->second)return false;
    if(!p.nonterminal.empty())return p.op.empty()&&p.children.empty()&&rs.nonterminals.contains(p.nonterminal);
    if(p.op.empty())return false;
    return std::all_of(p.children.begin(),p.children.end(),valid_pattern);
  };
  for(auto& r:rs.rules) {
    if(!ids.insert(r.id).second||r.cost<0||!rs.nonterminals.contains(r.lhs)||r.result!=r.lhs)return Result<int>::err({Error::Code::InvalidArgument,"invalid BURS rule: "+std::to_string(r.id)});
    if(r.immediate&&r.immediate->first>r.immediate->second)return Result<int>::err({Error::Code::InvalidArgument,"invalid immediate bounds"});
    if(r.pattern) {
      if(!valid_pattern(*r.pattern)||!r.pattern->nonterminal.empty())return Result<int>::err({Error::Code::InvalidArgument,"invalid structured BURS pattern"});
    }else {
      if(r.op.empty())return Result<int>::err({Error::Code::InvalidArgument,"empty rule operator"});
      for(auto& nt:r.operands)if(!rs.nonterminals.contains(nt))return Result<int>::err({Error::Code::InvalidArgument,"unknown child nonterminal: "+nt});
    }
  }
  return Result<int>::ok(0);
}
Result<Selection> select(const std::vector<Node>& nodes,NodeId root,const RuleSet& rs,std::string_view rootnt) {
  auto valid=validate(rs);if(!valid)return Result<Selection>::err(valid.error());
  if(!rs.nonterminals.contains(std::string(rootnt)))return Result<Selection>::err({Error::Code::InvalidArgument,"unknown root nonterminal"});
  std::map<NodeId,const Node*> map;
  for(auto& n:nodes)if(!map.emplace(n.id,&n).second)return Result<Selection>::err({Error::Code::InvalidArgument,"duplicate tree node"});
  if(!map.contains(root))return Result<Selection>::err({Error::Code::InvalidArgument,"missing tree root"});
  std::unordered_map<NodeId,unsigned> state,parents;
  std::vector<std::pair<NodeId,bool>> stack{{root,false}};std::vector<NodeId> postorder;
  while(!stack.empty()) {
    auto [id,done]=stack.back();stack.pop_back();
    if(!map.contains(id))return Result<Selection>::err({Error::Code::InvalidArgument,"unknown child node"});
    if(done){state[id]=2;postorder.push_back(id);continue;}
    if(state[id]==1)return Result<Selection>::err({Error::Code::Conflict,"cyclic selection tree"});
    if(state[id]==2)return Result<Selection>::err({Error::Code::Unsupported,"shared DAG requires an explicit treeification policy"});
    state[id]=1;stack.push_back({id,true});
    for(auto child:map.at(id)->children) {
      if(++parents[child]>1)return Result<Selection>::err({Error::Code::Unsupported,"shared DAG requires an explicit treeification policy"});
      stack.push_back({child,false});
    }
  }
  std::map<NodeId,std::map<std::string,Derivation>> dp;
  auto rules=rs.rules;std::sort(rules.begin(),rules.end(),[](auto& a,auto& b){return std::tie(a.priority,a.id)<std::tie(b.priority,b.id);});
  for(auto id:postorder)for(auto& r:rules) {
    const auto& n=*map.at(id);
    if((!r.type.empty()&&r.type!=n.type)||!legal_immediate(n,r.immediate))continue;
    Derivation d{r.id,r.cost,{},{},{}};int64_t cost=r.cost;
    std::function<bool(NodeId,const Pattern&)> match=[&](NodeId nid,const Pattern& pattern) {
      const auto& node=*map.at(nid);
      if((!pattern.type.empty()&&pattern.type!=node.type)||!legal_immediate(node,pattern.immediate))return false;
      if(!pattern.nonterminal.empty()) {
        auto it=dp[nid].find(pattern.nonterminal);if(it==dp[nid].end())return false;
        cost+=it->second.cost;d.children.push_back(nid);d.child_nt.push_back(pattern.nonterminal);return true;
      }
      if(node.op!=pattern.op||node.children.size()!=pattern.children.size())return false;
      d.covered.push_back(nid);
      for(size_t i=0;i<node.children.size();++i)if(!match(node.children[i],pattern.children[i]))return false;
      return true;
    };
    Pattern pattern;
    if(r.pattern)pattern=*r.pattern;
    else { pattern.op=r.op;for(auto& nt:r.operands)pattern.children.push_back(Pattern{"",nt}); }
    if(!match(id,pattern))continue;
    if(cost>std::numeric_limits<int>::max())return Result<Selection>::err({Error::Code::ResourceLimit,"BURS cost overflow"});
    d.cost=static_cast<int>(cost);
    auto it=dp[id].find(r.lhs);
    if(it==dp[id].end()||d.cost<it->second.cost)dp[id][r.lhs]=std::move(d);
  }
  if(!dp[root].contains(std::string(rootnt)))return Result<Selection>::err({Error::Code::Unsatisfiable,"node "+std::to_string(root)+" ("+map.at(root)->op+") cannot derive "+std::string(rootnt)});
  Selection s{std::string(rootnt),{},dp[root].at(std::string(rootnt)).cost,root};
  std::vector<std::pair<NodeId,std::string>> work{{root,std::string(rootnt)}};
  while(!work.empty()) {
    auto [id,nt]=work.back();work.pop_back();auto d=dp[id].at(nt);s.chosen[id]=d;
    for(size_t i=0;i<d.children.size();++i)work.push_back({d.children[i],d.child_nt[i]});
  }
  return Result<Selection>::ok(std::move(s));
}
Result<schedrow::Region> emit_scheduler(const std::vector<Node>& nodes,const RuleSet& rs,const Selection& selection) {
  std::set<NodeId> roots;
  for(auto&[id,d]:selection.chosen)roots.insert(id);
  for(auto&[id,d]:selection.chosen)for(auto child:d.children)roots.erase(child);
  if(roots.size()!=1||(selection.root&&*selection.root!=*roots.begin()))return Result<schedrow::Region>::err({Error::Code::InvalidArgument,"selection must have one derivation root"});
  auto checked=select(nodes,*roots.begin(),rs,selection.root_nt);
  if(!checked)return Result<schedrow::Region>::err(checked.error());
  if(checked.value().cost!=selection.cost||checked.value().chosen!=selection.chosen)return Result<schedrow::Region>::err({Error::Code::Conflict,"selection does not match its BURS derivation"});
  std::map<NodeId,const Node*> map;for(auto& n:nodes)map[n.id]=&n;
  std::map<RuleId,const Rule*> rules;for(auto& r:rs.rules)rules[r.id]=&r;
  schedrow::Region region{"limeburg"};
  std::vector<NodeId> ids;
  std::function<void(NodeId)> order=[&](NodeId id){for(auto child:selection.chosen.at(id).children)order(child);ids.push_back(id);};
  order(*roots.begin());
  for(auto id:ids) {
    const auto& d=selection.chosen.at(id);
    if(!map.contains(id)||!rules.contains(d.rule))return Result<schedrow::Region>::err({Error::Code::InvalidArgument,"invalid selected derivation"});
    const auto& rule=*rules.at(d.rule);
    if(rule.instruction.empty()) {
      if(!map.at(id)->has_imm||!map.at(id)->children.empty())return Result<schedrow::Region>::err({Error::Code::Unsupported,"non-emitting rules must lower immediate leaves"});
      if(id==*roots.begin())return Result<schedrow::Region>::err({Error::Code::Unsupported,"an immediate-only root has no machine instruction"});
      continue;
    }
    schedrow::Instruction i{};i.id=id;i.opcode=rule.instruction;i.defs={id};
    for(auto source:d.covered)if(map.at(source)->has_imm)i.immediates.emplace_back(source,map.at(source)->imm);
    for(auto child:d.children) {
      if(rules.at(selection.chosen.at(child).rule)->instruction.empty())i.immediates.emplace_back(child,map.at(child)->imm);
      else {i.uses.push_back(child);region.deps.push_back({child,id,schedrow::DepKind::True,0});}
    }
    i.origin="rule "+std::to_string(d.rule)+", source "+std::to_string(id);
    region.instructions.push_back(std::move(i));
  }
  return Result<schedrow::Region>::ok(std::move(region));
}
}
