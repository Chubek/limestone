#include "regtl.hpp"
namespace limestone::regtl {
Result<Allocation> linear_scan(const Program&p){
 auto rs=p.ranges;std::sort(rs.begin(),rs.end(),[](auto&a,auto&b){return a.begin<b.begin|| (a.begin==b.begin&&a.value<b.value);});
 Allocation a;struct Active{VReg v;uint32_t end;PReg r;};std::vector<Active> active;
 auto expire=[&](uint32_t b){active.erase(std::remove_if(active.begin(),active.end(),[&](auto&x){if(x.end<b)return true;return false;}),active.end());};
 for(auto&r:rs){expire(r.begin);std::vector<PReg> avail;for(auto&c:p.classes)if(c.name==r.klass)avail=c.members;
 for(auto x:r.constraint.forbidden)avail.erase(std::remove(avail.begin(),avail.end(),x),avail.end());
 if(r.constraint.fixed){if(std::any_of(active.begin(),active.end(),[&](auto&x){return x.r==*r.constraint.fixed;})) {if(!r.spillable)return Result<Allocation>::err({Error::Code::Unsatisfiable,"fixed register conflict"});a.spilled.push_back(r.value);continue;}a.regs[r.value]=*r.constraint.fixed;active.push_back({r.value,r.end,*r.constraint.fixed});continue;}
 avail.erase(std::remove_if(avail.begin(),avail.end(),[&](PReg x){return std::any_of(active.begin(),active.end(),[&](auto&a){return a.r==x;});}),avail.end());
 if(avail.empty()){if(!r.spillable)return Result<Allocation>::err({Error::Code::Unsatisfiable,"register pressure exceeds class"});a.spilled.push_back(r.value);continue;}
 auto reg=*std::min_element(avail.begin(),avail.end());a.regs[r.value]=reg;active.push_back({r.value,r.end,reg});
 }
 return Result<Allocation>::ok(std::move(a));
}
Result<int> verify(const Program&p,const Allocation&a){for(auto&r:p.ranges){if(a.spilled.end()!=std::find(a.spilled.begin(),a.spilled.end(),r.value))continue;auto it=a.regs.find(r.value);if(it==a.regs.end())return Result<int>::err({Error::Code::Conflict,"unassigned value"});if(r.constraint.fixed&&it->second!=*r.constraint.fixed)return Result<int>::err({Error::Code::Conflict,"fixed-register violation"});if(std::find(r.constraint.forbidden.begin(),r.constraint.forbidden.end(),it->second)!=r.constraint.forbidden.end())return Result<int>::err({Error::Code::Conflict,"forbidden-register violation"});}for(size_t i=0;i<p.ranges.size();++i)for(size_t j=i+1;j<p.ranges.size();++j)if(p.ranges[i].end>=p.ranges[j].begin&&p.ranges[j].end>=p.ranges[i].begin){auto a1=a.regs.find(p.ranges[i].value),a2=a.regs.find(p.ranges[j].value);if(a1!=a.regs.end()&&a2!=a.regs.end()&&a1->second==a2->second)return Result<int>::err({Error::Code::Conflict,"interference violation"});}return Result<int>::ok(0);}
std::string print(const Program&p){std::string s="RegTL\n";for(auto&r:p.ranges)s+="v"+std::to_string(r.value)+" ["+std::to_string(r.begin)+","+std::to_string(r.end)+"] "+r.klass+"\n";return s;}
}
