#include "limeburg.hpp"
namespace limestone::limeburg {
namespace {
using Key=std::pair<NodeId,std::string>;
struct KeyHash{size_t operator()(const Key&k)const{return std::hash<uint64_t>{}((uint64_t(k.first)<<32)^std::hash<std::string>{}(k.second));}};
}
Result<Selection> select(const std::vector<Node>&nodes,NodeId root,const RuleSet&rs,std::string_view rootnt){
 std::unordered_map<NodeId,const Node*> map;for(auto&n:nodes)map[n.id]=&n;
 std::unordered_map<Key,Derivation,KeyHash> dp;
 std::function<std::optional<int>(NodeId,std::string)> solve=[&](NodeId id,std::string nt)->std::optional<int>{
   Key key{id,nt};if(auto it=dp.find(key);it!=dp.end())return it->second.cost;
   auto ni=map.find(id);if(ni==map.end())return{};const Node&n=*ni->second;std::optional<int> best;Derivation bd{};
   for(auto&r:rs.rules)if(r.lhs==nt&&r.op==n.op&&r.operands.size()==n.children.size()){
     int c=r.cost;bool ok=true;std::vector<std::string> cns;
     for(size_t i=0;i<n.children.size();++i){auto x=solve(n.children[i],r.operands[i]);if(!x){ok=false;break;}c+=*x;cns.push_back(r.operands[i]);}
     if(ok&&(!best||c<*best||(c==*best&&(r.priority<bd.rule)))){best=c;bd={r.id,c,cns};}
   }
   if(best)dp.emplace(key,bd);return best;
 };
 auto c=solve(root,std::string(rootnt));if(!c)return Result<Selection>::err({Error::Code::Unsatisfiable,"no BURS derivation for root"});
 Selection s; s.root_nt=std::string(rootnt);s.cost=*c;
 for(auto&[k,d]:dp)if(k.first==root||d.cost>=0)s.chosen[k.first]=d;
 return Result<Selection>::ok(std::move(s));
}
}
