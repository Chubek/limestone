module machineir.exchange;

import machineir.ir;
import machineir.analysis : verify, recomputePredecessors, analyzeDominance, dominates;
import std.json : JSONValue, JSONType, JSONOptions, parseJSON, toJSON;
import std.conv : to;
import std.typecons : Nullable, tuple;
import std.math : isFinite;
import std.algorithm : canFind, sort;

/** Versioned region/CFG C++/D exchange with an owning metadata envelope.
 *  The program uses virtual identities; allocations stay in a separate contract.
 */
struct RegionExchange {
    MachineModule program;
    private JSONValue envelope;
    private uint[] instructionIds;
    private uint[string] valueIds;
    ulong frameSize() const {auto frame="spill_frame" in envelope.object;return frame is null?0:wideUnsigned((*frame)["size"]);}
    string spillFrameContract() const {auto frame="spill_frame" in envelope.object;return frame is null?"":toJSON(*frame);}
    size_t groupCount() const {auto groups="groups" in envelope.object;return groups is null?0:(*groups).array.length;}
    string groupContract(size_t index) const {
        if(index>=groupCount)throw new ExchangeError("group index out of range");
        return toJSON(envelope["groups"][index]);
    }

    uint instructionId(size_t index) const {
        if(index>=instructionIds.length)throw new ExchangeError("instruction index out of range");
        return instructionIds[index];
    }
    Nullable!uint physicalRegister(uint value) const {
        auto allocation="allocation" in envelope.object;
        if(allocation !is null)foreach(r;(*allocation)["registers"].array)
            if(unsigned(r[0])==value)return Nullable!uint(unsigned(r[1]));
        return Nullable!uint.init;
    }
    /** Preserve the complete scheduling/selection contract for inspection. */
    string instructionContract(size_t index) const {
        if(index>=instructionIds.length)throw new ExchangeError("instruction index out of range");
        return toJSON(envelope["instructions"][index]);
    }
}
class ExchangeError : Exception {
    this(string message){super("MachineIR exchange: "~message);}
}
private uint unsigned(const ref JSONValue v) {
    auto n=wideUnsigned(v);
    if(n>uint.max)throw new ExchangeError("integer exceeds 32 bits");return cast(uint)n;
}
private ulong wideUnsigned(const ref JSONValue v) {
    if(v.type==JSONType.uinteger)return v.uinteger;
    if(v.type==JSONType.integer&&v.integer>=0)return cast(ulong)v.integer;
    throw new ExchangeError("expected unsigned integer");
}
private long signed(const ref JSONValue v) {
    if(v.type==JSONType.integer)return v.integer;
    if(v.type==JSONType.uinteger&&v.uinteger<=long.max)return cast(long)v.uinteger;
    throw new ExchangeError("expected signed 64-bit integer");
}
private bool flag(const ref JSONValue v) {
    if(v.type==JSONType.true_)return true;if(v.type==JSONType.false_)return false;
    throw new ExchangeError("expected Boolean");
}
private RegisterRef reference(const ref RegionExchange exchange,const ref JSONValue v) {
    auto id=unsigned(v);auto function_=exchange.program.functions[0];
    foreach(r;function_.virtualRegisters)if(r.number==id)return r;
    throw new ExchangeError("unknown virtual register");
}
private string[] effects(const ref JSONValue values) {
    string[] names;foreach(v;values.array)names~="p"~to!string(unsigned(v));return names;
}
private const(MachineBasicBlock) findBlock(const(MachineFunction) f,uint id) {
    foreach(b;f.blocks)if(b.id==id)return b;
    throw new ExchangeError("unknown instruction block");
}
private void validateJSON(const ref JSONValue v,size_t depth=0) {
    if(depth>256)throw new ExchangeError("JSON nesting limit");
    if(v.type==JSONType.array)foreach(c;v.array)validateJSON(c,depth+1);
    else if(v.type==JSONType.object)foreach(c;v.object)validateJSON(c,depth+1);
    else if(v.type==JSONType.null_||v.type==JSONType.float_)throw new ExchangeError("null/floating metadata is unsupported");
}
private bool[uint] identities(const ref JSONValue values) {
    bool[uint] result;foreach(v;values.array)result[unsigned(v)]=true;return result;
}
private uint identityKey(string text) {
    if(!text.length)throw new ExchangeError("empty identity key");
    foreach(c;text)if(c<'0'||c>'9')throw new ExchangeError("invalid identity key");
    if(text.length>1&&text[0]=='0')throw new ExchangeError("noncanonical identity key");
    try{return to!uint(text);}catch(Exception){throw new ExchangeError("identity key exceeds 32 bits");}
}
private void known(const ref JSONValue value,string[] fields) {
    foreach(key;value.object.keys)if(!fields.canFind(key))throw new ExchangeError("unknown exchange field: "~key);
}
private void sourceMetadata(const ref JSONValue sources) {
    foreach(key,metadata;sources.object) {
        identityKey(key);
        known(metadata,["strings","properties"]);
        metadata["properties"].object;
        bool[uint] indices;
        foreach(argument;metadata["strings"].array) {
            known(argument,["index","value"]);
            auto index=unsigned(argument["index"]);
            if(index in indices||argument["value"].str.canFind('\0'))
                throw new ExchangeError("invalid or duplicate string argument");
            indices[index]=true;
        }
    }
}
// std.json keeps the last member of a repeated key. Check the validated token
// stream as well so escaped spellings cannot change the exchange contract.
private void uniqueKeys(string source) {
    bool[string][] objects;
    for(size_t p=0;p<source.length;++p) {
        if(source[p]=='{')objects~=cast(bool[string])null;
        else if(source[p]=='}')objects.length--;
        else if(source[p]=='"') {
            auto begin=p++;
            while(source[p]!='"'){if(source[p]=='\\')++p;++p;}
            auto end=p+1,next=end;
            while(next<source.length&&" \t\n\r".canFind(source[next]))++next;
            if(next<source.length&&source[next]==':') {
                auto key=parseJSON(source[begin..end],JSONOptions.strictParsing).str;
                if(key in objects[$-1])throw new ExchangeError("duplicate JSON field: "~key);
                objects[$-1][key]=true;
            }
        }
    }
}
private double positiveReal(const ref JSONValue value) {
    double n;try{n=to!double(value.str);}catch(Exception){throw new ExchangeError("expected a real quantity encoded as a string");}
    if(!isFinite(n)||n<=0)throw new ExchangeError("invalid scheduling quantity");return n;
}
private uint maximumLatency(const ref JSONValue range) {
    if(range.array.length!=2)throw new ExchangeError("latency range needs two bounds");auto lower=unsigned(range[0]),upper=unsigned(range[1]);if(lower>upper)throw new ExchangeError("reversed latency range");return upper;
}
private uint resultLatency(const ref JSONValue instruction_,uint value,const ref JSONValue consumer,uint useIndex,bool implicit=false) {
    if("operand_latencies" in instruction_.object)foreach(t;instruction_["operand_latencies"].array)
        if(unsigned(t["result"])==value&&flag(t["implicit"])==implicit&&t["consumer"].str==consumer["opcode"].str&&unsigned(t["use"])==useIndex)return maximumLatency(t["cycles"]);
    auto rangeKey=implicit?"implicit_result_latency_ranges":"result_latency_ranges",scalarKey=implicit?"implicit_result_latency":"result_latency";
    auto ranges=rangeKey in instruction_.object;if(ranges !is null){auto range=to!string(value) in (*ranges).object;if(range !is null)return maximumLatency(*range);}
    auto scalars=scalarKey in instruction_.object;if(scalars !is null){auto latency=to!string(value) in (*scalars).object;if(latency !is null)return unsigned(*latency);}
    return ("latency_range" in instruction_.object)?maximumLatency(instruction_["latency_range"]):unsigned(instruction_["latency"]);
}
/** The envelope is an SSA handoff, unlike the general mutable-register IR.
 *  Validate its dataflow, effects and schedule without guessing machine capacities.
 */
private void validateContracts(ref RegionExchange x) {
    auto root=x.envelope;auto f=x.program.functions[0];auto dom=analyzeDominance(f);
    auto hasBlocks=("blocks" in root.object)!is null;
    size_t[uint] positions,definitions,issuePositions;uint[uint] instructionBlocks,cycles;
    foreach(k,i;root["instructions"].array) {
        auto id=unsigned(i["id"]);positions[id]=k;auto block=hasBlocks?unsigned(i["block"]):f.blocks[0].id;instructionBlocks[id]=block;
        auto defs=identities(i["defs"]),uses=identities(i["uses"]);
        if(defs.length!=i["defs"].array.length)throw new ExchangeError("duplicate instruction definition");
        foreach(value;defs.keys){if(value in definitions)throw new ExchangeError("multiple definitions of a virtual value");definitions[value]=k;}
        foreach(v;i["early_defs"].array)if(!(unsigned(v) in defs))throw new ExchangeError("early definition is not an instruction result");
        foreach(t;i["ties"].array)if(t.array.length!=2||!(unsigned(t[0]) in defs)||!(unsigned(t[1]) in uses))throw new ExchangeError("tie needs a definition and use");
        auto implicitDefs=identities(i["implicit_defs"]);identities(i["implicit_uses"]);
        foreach(key,n;i["result_latency"].object){auto value=identityKey(key);if(!(value in defs))throw new ExchangeError("latency is not an explicit instruction result");unsigned(n);}
        if("implicit_result_latency" in i.object){if(unsigned(root["version"])<2)throw new ExchangeError("implicit result latency needs version 2");foreach(key,n;i["implicit_result_latency"].object){auto value=identityKey(key);if(!(value in implicitDefs))throw new ExchangeError("latency is not an implicit instruction result");unsigned(n);}}
        foreach(key;["latency_range","memory_latency","result_latency_ranges","implicit_result_latency_ranges","operand_latencies"])if((key in i.object)&&unsigned(root["version"])<5)throw new ExchangeError("extended timing needs version 5");
        if("source_metadata" in i.object){if(unsigned(root["version"])<6)throw new ExchangeError("source metadata needs version 6");sourceMetadata(i["source_metadata"]);}
        if("latency_range" in i.object)maximumLatency(i["latency_range"]);
        if("memory_latency" in i.object){maximumLatency(i["memory_latency"]);if(!flag(i["memory"])&&!("access" in i.object))throw new ExchangeError("memory timing needs memory effects");}
        foreach(key;["result_latency_ranges","implicit_result_latency_ranges"])if(key in i.object)foreach(value,n;i[key].object){auto id_=identityKey(value);bool implicit=key=="implicit_result_latency_ranges";auto scalarKey=implicit?"implicit_result_latency":"result_latency";if(!(id_ in (implicit?implicitDefs:defs))||((scalarKey in i.object)&&(value in i[scalarKey].object)))throw new ExchangeError("invalid or duplicate result timing");maximumLatency(n);}
        bool[string] operandTimings;if("operand_latencies" in i.object)foreach(t;i["operand_latencies"].array){known(t,["result","consumer","use","cycles","implicit"]);auto result=unsigned(t["result"]),index=unsigned(t["use"]);bool implicit=flag(t["implicit"]);auto consumer=t["consumer"].str;maximumLatency(t["cycles"]);auto key=to!string(result)~":"~to!string(index)~":"~to!string(implicit)~":"~consumer;if(!consumer.length||!(result in (implicit?implicitDefs:defs))||key in operandTimings)throw new ExchangeError("invalid or duplicate operand timing");operandTimings[key]=true;}
        if("register_classes" in i.object)foreach(key,klass;i["register_classes"].object){auto value=identityKey(key);if(!klass.str.length||(!(value in defs)&&!(value in uses)))throw new ExchangeError("register class is not an instruction operand");}
        unsigned(i["latency"]);positiveReal(i["throughput"]);auto priority=signed(i["priority"]);if(priority<int.min||priority>int.max)throw new ExchangeError("priority exceeds signed 32 bits");
        auto issueWidth=("issue_width" in i.object)?unsigned(i["issue_width"]):1;
        if(("issue_width" in i.object)&&unsigned(root["version"])<4)throw new ExchangeError("instruction issue width needs version 4");
        if(!issueWidth)throw new ExchangeError("zero instruction issue width");
        auto issueSlots=identities(i["issue_slots"]);if(issueSlots.length!=i["issue_slots"].array.length)throw new ExchangeError("duplicate instruction issue slot");
        if(issueSlots.length&&issueWidth>issueSlots.length)throw new ExchangeError("instruction issue width exceeds legal slots");
        foreach(key,n;i["pressure_delta"].object){auto delta=signed(n);if(!key.length||delta<int.min||delta>int.max)throw new ExchangeError("invalid register-pressure quantity");}
        foreach(r;i["resources"].array){if(!unsigned(r["duration"])||(!r["resource"].str.length&&!r["alternatives"].array.length))throw new ExchangeError("invalid resource reservation");positiveReal(r["quantity"]);unsigned(r["offset"]);foreach(a;r["alternatives"].array)if(!a.str.length)throw new ExchangeError("empty resource alternative");}
        flag(i["memory"]);flag(i["speculative"]);flag(i["barrier"]);flag(i["may_trap"]);
        auto call=flag(i["call"]),terminal=flag(i["terminator"]);
        string control=("control" in i.object)?i["control"].str:"none";
        if((control=="call"&&!call)||(control!="none"&&control!="call"&&!terminal))throw new ExchangeError("control flow disagrees with instruction effects");
        bool[uint] targets;if("block_targets" in i.object){targets=identities(i["block_targets"]);if(targets.length!=i["block_targets"].array.length)throw new ExchangeError("duplicate block target");}
        if((control=="branch"&&targets.length!=1)||(control=="conditional_branch"&&targets.length!=2)||((control=="return"||control=="trap")&&targets.length)||(!terminal&&targets.length))throw new ExchangeError("invalid control-flow targets");
        if(hasBlocks&&terminal){bool[uint] successors;foreach(s;findBlock(f,block).successors)successors[s]=true;if(targets!=successors)throw new ExchangeError("terminator targets disagree with CFG successors");}
        auto access="access" in i.object;if(access !is null){auto a=*access;auto alignment=unsigned(a["alignment"]);if(alignment&&(alignment&(alignment-1)))throw new ExchangeError("invalid memory alignment");auto ordering=a["ordering"].str;if(ordering!="relaxed"&&ordering!="acquire"&&ordering!="release"&&ordering!="acq_rel"&&ordering!="seq_cst")throw new ExchangeError("invalid memory ordering");}
    }
    bool available(uint value,uint block,size_t position) {
        auto definition=value in definitions;if(definition is null)return true;
        auto producer=root["instructions"][*definition];auto source=instructionBlocks[unsigned(producer["id"])];
        return source==block?*definition<position:dominates(dom,source,block);
    }
    foreach(k,i;root["instructions"].array)foreach(v;i["uses"].array)if(!available(unsigned(v),instructionBlocks[unsigned(i["id"])],k))throw new ExchangeError("definition does not dominate its use");
    foreach(b;f.blocks)foreach(r;b.liveOut)if(!available(r.number,b.id,root["instructions"].array.length))throw new ExchangeError("definition does not dominate a live-out");
    bool[string] slots;
    size_t[uint] blockLayout;foreach(k,b;f.blocks)blockLayout[b.id]=k;
    size_t priorIssueBlock;bool[uint] issueTerminated;
    foreach(k,s;root["schedule"].array) {
        auto id=unsigned(s["id"]),block=instructionBlocks[id];auto rank=blockLayout[block];
        if(k&&rank<priorIssueBlock)throw new ExchangeError("issue order crosses CFG block layout");
        priorIssueBlock=rank;
        if(block in issueTerminated)throw new ExchangeError("issue follows a terminator");
        if(flag(root["instructions"][positions[id]]["terminator"]))issueTerminated[block]=true;
        issuePositions[id]=k;
    }
    foreach(s;root["schedule"].array){
        auto id=unsigned(s["id"]),cycle=unsigned(s["cycle"]);cycles[id]=cycle;auto i=root["instructions"][positions[id]];
        auto width=("issue_width" in i.object)?unsigned(i["issue_width"]):1;
        uint[] assigned;auto slot="slot" in s.object;if(slot !is null)assigned~=unsigned(*slot);
        if("additional_slots" in s.object){if(unsigned(root["version"])<4)throw new ExchangeError("additional issue slots need version 4");foreach(v;s["additional_slots"].array)assigned~=unsigned(v);if(slot is null&&assigned.length)throw new ExchangeError("additional slots require a primary slot");}
        if(assigned.length&&assigned.length!=width)throw new ExchangeError("incomplete issue-slot assignment");
        if(!assigned.length&&(i["issue_slots"].array.length||width>1))throw new ExchangeError("missing issue-slot assignment");
        auto allowed=identities(i["issue_slots"]);
        foreach(selected;assigned){auto key=to!string(instructionBlocks[id])~":"~to!string(cycle)~":"~to!string(selected);if((allowed.length&&!(selected in allowed))||key in slots)throw new ExchangeError("invalid issue-slot assignment");slots[key]=true;}
        auto resources=s["resources"].array;if(resources.length){if(resources.length!=i["resources"].array.length)throw new ExchangeError("incomplete resource assignment");foreach(k,selected;resources){auto reservation=i["resources"][k];auto alternatives=reservation["alternatives"].array;bool accepted=alternatives.length?false:selected.str==reservation["resource"].str;foreach(a;alternatives)accepted|=a.str==selected.str;if(!accepted)throw new ExchangeError("invalid resource assignment");}}
    }
    if("groups" in root.object) {
        if(unsigned(root["version"])<3)throw new ExchangeError("groups need version 3");
        struct IssueRank {uint id,cycle;size_t source;}
        IssueRank[] issueOrder;size_t[uint] scheduledPositions;JSONValue[uint] issues;
        foreach(k,s;root["schedule"].array){auto id=unsigned(s["id"]);issueOrder~=IssueRank(id,unsigned(s["cycle"]),k);issues[id]=s;}
        sort!((a,b)=>tuple(instructionBlocks[a.id],a.cycle,a.source)<tuple(instructionBlocks[b.id],b.cycle,b.source))(issueOrder);
        foreach(k,s;issueOrder)scheduledPositions[s.id]=k;
        bool[uint] groupIds;
        foreach(g;root["groups"].array) {
            known(g,["id","kind","members","name","origin","pattern","benefit","issue_width","issue_slots"]);
            auto id=unsigned(g["id"]),width=unsigned(g["issue_width"]);auto kind=g["kind"].str;
            if(id in groupIds)throw new ExchangeError("duplicate scheduling group");groupIds[id]=true;
            if(!["ordered","adjacent","same_cycle","bundle","atomic","fusion","pair"].canFind(kind))throw new ExchangeError("unknown scheduling group kind");
            bool same=kind=="same_cycle"||kind=="bundle",adjacent=kind!="ordered"&&kind!="same_cycle";
            auto members=g["members"].array;auto allowed=identities(g["issue_slots"]);
            if(!members.length||(kind=="pair"&&members.length!=2)||(kind=="fusion"&&members.length<2))throw new ExchangeError("invalid scheduling group arity");
            if(allowed.length!=g["issue_slots"].array.length)throw new ExchangeError("duplicate group issue slot");
            if((width||allowed.length)&&!same)throw new ExchangeError("group issue constraints need a same-cycle group");
            auto benefit=signed(g["benefit"]);if(benefit<int.min||benefit>int.max)throw new ExchangeError("group benefit exceeds signed 32 bits");g["name"].str;g["origin"].str;g["pattern"].str;
            bool[uint] seen;uint block,previous;ulong slotDemand;
            foreach(k,v;members) {
                auto member=unsigned(v);if(!(member in positions)||member in seen)throw new ExchangeError("unknown or duplicate group member");seen[member]=true;
                auto operation=root["instructions"][positions[member]];slotDemand+=("issue_width" in operation.object)?unsigned(operation["issue_width"]):1;
                if(!k)block=instructionBlocks[member];else if(block!=instructionBlocks[member])throw new ExchangeError("group crosses a block boundary");
                if(k&&(positions[member]<=positions[previous]||(adjacent&&positions[member]!=positions[previous]+1)))throw new ExchangeError("group order/adjacency violation");
                if(cycles.length) {
                    if(k&&(cycles[member]<cycles[previous]||(same&&cycles[member]!=cycles[previous])))throw new ExchangeError("group cycle violation");
                    if(k&&(scheduledPositions[member]<=scheduledPositions[previous]||(adjacent&&scheduledPositions[member]!=scheduledPositions[previous]+1)))throw new ExchangeError("scheduled group order/adjacency violation");
                    if(k&&(issuePositions[member]<=issuePositions[previous]||(adjacent&&issuePositions[member]!=issuePositions[previous]+1)))throw new ExchangeError("issue group order/adjacency violation");
                    if(allowed.length){auto issue=issues[member];auto slot="slot" in issue.object;if(slot is null||!(unsigned(*slot) in allowed))throw new ExchangeError("group issue-slot violation");}
                    if(allowed.length&&("additional_slots" in issues[member].object))foreach(extra;issues[member]["additional_slots"].array)if(!(unsigned(extra) in allowed))throw new ExchangeError("group issue-slot violation");
                }
                previous=member;
            }
            if(width&&slotDemand>width)throw new ExchangeError("group issue width is too small");
        }
    }
    void dependency(uint a,uint b,uint latency) {
        if(instructionBlocks[a]!=instructionBlocks[b]){if(!dominates(dom,instructionBlocks[a],instructionBlocks[b]))throw new ExchangeError("dependency crosses unrelated CFG paths");return;}
        if(positions[a]>=positions[b])throw new ExchangeError("instruction order violates a dependency");
        if(cycles.length&&cast(ulong)cycles[a]+latency>cycles[b])throw new ExchangeError("schedule violates dependency latency");
        if(cycles.length&&issuePositions[a]>=issuePositions[b])throw new ExchangeError("issue order violates a dependency");
    }
    foreach(d;root["dependencies"].array){auto kind=d["kind"].str;if(kind!="true"&&kind!="anti"&&kind!="output"&&kind!="memory"&&kind!="control"&&kind!="ordering")throw new ExchangeError("unknown dependency kind");flag(d["scheduler_only"]);auto latency=unsigned(d["latency"]);if("latency_range" in d.object){if(unsigned(root["version"])<5)throw new ExchangeError("dependency ranges need version 5");latency=maximumLatency(d["latency_range"]);}if(!unsigned(d["distance"]))dependency(unsigned(d["producer"]),unsigned(d["consumer"]),latency);}
    size_t[uint][uint] physicalWriters;size_t[][uint][uint] physicalReaders;
    foreach(k,i;root["instructions"].array) {
        auto id=unsigned(i["id"]),block=instructionBlocks[id];
        foreach(index,v;i["uses"].array){auto value=unsigned(v);auto definition=value in definitions;if(definition !is null){auto p=root["instructions"][*definition];dependency(unsigned(p["id"]),id,resultLatency(p,value,i,cast(uint)index));}}
        foreach(index,v;i["implicit_uses"].array){auto value=unsigned(v);auto writers=block in physicalWriters;if(writers !is null){auto definition=value in *writers;if(definition !is null){auto p=root["instructions"][*definition];dependency(unsigned(p["id"]),id,resultLatency(p,value,i,cast(uint)index,true));}}physicalReaders[block][value]~=k;}
        foreach(v;i["implicit_defs"].array) {
            auto value=unsigned(v);auto writers=block in physicalWriters;
            if(writers !is null){auto previous=value in *writers;if(previous !is null)dependency(unsigned(root["instructions"][*previous]["id"]),id,0);}
            auto readers=block in physicalReaders;
            if(readers !is null){auto previous=value in *readers;if(previous !is null)foreach(reader;*previous)if(reader!=k)dependency(unsigned(root["instructions"][reader]["id"]),id,0);}
            physicalReaders[block][value]=cast(size_t[])null;physicalWriters[block][value]=k;
        }
        foreach(p;root["instructions"].array[0..k])if(instructionBlocks[unsigned(p["id"])]==block) {
            bool ordered=flag(p["barrier"])||flag(p["call"])||flag(i["barrier"])||flag(i["call"])||flag(i["terminator"])||(flag(p["may_trap"])&&!flag(i["speculative"]))||(flag(i["may_trap"])&&!flag(p["speculative"]));
            auto memory(const ref JSONValue instruction_) {auto access="access" in instruction_.object;return access is null?JSONValue(["read":JSONValue(flag(instruction_["memory"])),"write":JSONValue(flag(instruction_["memory"])),"volatile":JSONValue(false),"atomic":JSONValue(false),"ordering":JSONValue("relaxed"),"address_space":JSONValue(""),"alias_sets":JSONValue(cast(JSONValue[])[]) ]):*access;}
            auto a=memory(p),b=memory(i);bool activeA=flag(a["read"])||flag(a["write"])||flag(a["volatile"])||a["ordering"].str!="relaxed",activeB=flag(b["read"])||flag(b["write"])||flag(b["volatile"])||b["ordering"].str!="relaxed",memoryDependency=false;
            if(activeA&&activeB){bool memoryOrder=flag(a["volatile"])||flag(b["volatile"])||["acquire","acq_rel","seq_cst"].canFind(a["ordering"].str)||["release","acq_rel","seq_cst"].canFind(b["ordering"].str);bool alias_=!a["alias_sets"].array.length||!b["alias_sets"].array.length;foreach(v;a["alias_sets"].array)foreach(w;b["alias_sets"].array)alias_|=unsigned(v)==unsigned(w);if(a["address_space"].str.length&&b["address_space"].str.length&&a["address_space"].str!=b["address_space"].str)alias_=false;memoryDependency=memoryOrder||((flag(a["write"])||flag(b["write"])||(flag(a["atomic"])&&flag(b["atomic"])))&&alias_);ordered|=memoryDependency;}
            if(ordered){auto latency=(memoryDependency&&("memory_latency" in p.object))?maximumLatency(p["memory_latency"]):0;dependency(unsigned(p["id"]),id,latency);}
        }
    }
}
private MachineInstruction instruction(const ref RegionExchange x,const ref JSONValue v) {
    MachineInstruction i;i.opcode=Opcode.generic(v["opcode"].str);i.opcode.semanticClass=v["semantic_class"].str;i.sourceName=v["origin"].str;
    foreach(r;v["defs"].array)i.results~=Operand.reg(reference(x,r));
    foreach(r;v["uses"].array)i.operands~=Operand.reg(reference(x,r));
    foreach(n;v["immediates"].array)i.operands~=Operand.imm(signed(n["integer"]));
    if("block_targets" in v.object)foreach(target;v["block_targets"].array)i.operands~=Operand.block(unsigned(target));
    i.effects.implicitUses=effects(v["implicit_uses"]);i.effects.implicitDefs=effects(v["implicit_defs"]);
    i.effects.mayTrap=flag(v["may_trap"]);i.effects.serializing=flag(v["barrier"]);
    if(flag(v["call"]))i.effects.control=ControlFlowKind.call;
    else if(flag(v["terminator"]))i.effects.control=ControlFlowKind.unknown;
    else if(i.effects.serializing)i.effects.control=ControlFlowKind.barrier;
    if("control" in v.object)switch(v["control"].str){case "none":break;case "branch":i.effects.control=ControlFlowKind.unconditionalBranch;break;case "conditional_branch":i.effects.control=ControlFlowKind.conditionalBranch;break;case "return":i.effects.control=ControlFlowKind.return_;break;case "indirect_branch":i.effects.control=ControlFlowKind.indirectBranch;break;case "trap":i.effects.control=ControlFlowKind.trap;break;case "call":i.effects.control=ControlFlowKind.call;break;default:throw new ExchangeError("unknown control flow");}
    auto access="access" in v.object;
    if(access !is null){auto read=flag((*access)["read"]),write=flag((*access)["write"]);i.effects.memory=read?(write?MemoryEffect.readWrite:MemoryEffect.read):(write?MemoryEffect.write:MemoryEffect.none);i.effects.atomic=flag((*access)["atomic"]);
        MemoryOperand m;m.isLoad=read;m.isStore=write;m.atomic=i.effects.atomic;m.volatileAccess=flag((*access)["volatile"]);m.addressSpaceName=(*access)["address_space"].str;m.ordering=(*access)["ordering"].str;m.size=unsigned((*access)["size"]);m.alignment=to!string(unsigned((*access)["alignment"]));foreach(alias_;(*access)["alias_sets"].array)m.aliasSets~=unsigned(alias_);i.effects.memoryAccesses=[m];
    }
    else if(flag(v["memory"]))i.effects.memory=MemoryEffect.unknown;
    foreach(t;v["ties"].array)i.constraints~="tie %v"~to!string(unsigned(t[0]))~" %v"~to!string(unsigned(t[1]));
    foreach(r;v["early_defs"].array)i.constraints~="early-def %v"~to!string(unsigned(r));
    return i;
}
/** Read versions 1 through 6; scheduling and source contracts are owned. */
RegionExchange readExchange(string source) {
    if(source.length>4*1024*1024)throw new ExchangeError("JSON input size limit");
    try{return readEnvelope(source);}catch(ExchangeError error){throw error;}catch(Exception error){throw new ExchangeError(error.msg);}
}
private RegionExchange readEnvelope(string source) {
    RegionExchange x;x.envelope=parseJSON(source,256,JSONOptions.strictParsing);uniqueKeys(source);validateJSON(x.envelope);
    auto root=x.envelope;
    known(root,["schema","version","module","target","function","region","values","instructions","dependencies","schedule","outputs","allocation","blocks","entry","spill_frame","groups"]);
    auto schemaVersion=unsigned(root["version"]);if(root["schema"].str!="limestone.machineir.region"||(schemaVersion<1||schemaVersion>6))throw new ExchangeError("unsupported schema version");
    x.program.name=root["module"].str;x.program.config.name=root["target"].str;
    if(!x.program.name.length||!x.program.config.name.length||!root["function"].str.length)throw new ExchangeError("missing exchange identity");
    root["region"].str;if(("entry" in root.object)&&!("blocks" in root.object))throw new ExchangeError("entry needs a CFG");
    auto f=new MachineFunction(root["function"].str);x.program.functions=[f];MachineBasicBlock[uint] blocks;
    if("blocks" in root.object){if(schemaVersion<2)throw new ExchangeError("CFG needs version 2");foreach(v;root["blocks"].array){known(v,["id","name","successors","live_out"]);auto id=unsigned(v["id"]);if(id in blocks)throw new ExchangeError("duplicate block");auto b=f.importBlock(id,v["name"].str);blocks[id]=b;foreach(s;v["successors"].array)b.successors~=unsigned(s);}auto entry=unsigned(root["entry"]);if(!(entry in blocks))throw new ExchangeError("unknown entry block");if(!f.blocks.length||f.blocks[0].id!=entry)throw new ExchangeError("entry must be first in block layout");}
    else{auto b=f.newBlock(root["region"].str);blocks[b.id]=b;}
    bool[uint] values,ids;
    foreach(v;root["values"].array) {
        known(v,["id","type","class"]);
        auto id=unsigned(v["id"]);if(id in values)throw new ExchangeError("duplicate value");values[id]=true;
        auto r=RegisterRef.vreg("v"~to!string(id),v["class"].str);r.number=id;
        // Width remains unknown when no target-independent scalar type supplies it.
        auto type=v["type"].str;
        if(type.length>1&&(type[0]=='i'||type[0]=='u'||type[0]=='f')) {
            bool digits=true;foreach(c;type[1..$])if(c<'0'||c>'9')digits=false;
            if(digits)r.width=to!uint(type[1..$]);
        }
        f.virtualRegisters~=r;x.valueIds[r.name]=id;
    }
    size_t[uint] blockOrder;foreach(k,b;f.blocks)blockOrder[b.id]=k;size_t priorBlock;bool[uint] terminated;
    foreach(v;root["instructions"].array){known(v,["id","opcode","defs","uses","immediates","origin","latency","implicit_defs","implicit_uses","barrier","call","terminator","may_trap","memory","speculative","semantic_class","opcode_class","early_defs","ties","throughput","resources","issue_slots","priority","pressure_delta","result_latency","access","block","block_targets","control","register_classes","implicit_result_latency","issue_width","latency_range","memory_latency","result_latency_ranges","implicit_result_latency_ranges","operand_latencies","source_metadata"]);auto id=unsigned(v["id"]);if(id in ids)throw new ExchangeError("duplicate instruction");if(!v["opcode"].str.length)throw new ExchangeError("empty instruction opcode");ids[id]=true;x.instructionIds~=id;
        if(schemaVersion==1){if(("block" in v.object)||("block_targets" in v.object)||("control" in v.object))throw new ExchangeError("control-flow fields need version 2");}else{unsigned(v["block"]);v["control"].str;v["block_targets"].array;if(!("blocks" in root.object)&&v["block_targets"].array.length)throw new ExchangeError("branch targets need a CFG");}
        auto blockId=("blocks" in root.object)?unsigned(v["block"]):0;if(!(blockId in blocks))throw new ExchangeError("unknown instruction block");auto blockIndex=blockOrder[blockId];if(blockIndex<priorBlock)throw new ExchangeError("instruction order crosses block layout");priorBlock=blockIndex;if(blockId in terminated)throw new ExchangeError("instruction follows a terminator");if(flag(v["terminator"]))terminated[blockId]=true;
        foreach(r;v["resources"].array)known(r,["resource","duration","quantity","offset","alternatives"]);foreach(i;v["immediates"].array){known(i,["value","integer"]);unsigned(i["value"]);}if("access" in v.object)known(v["access"],["read","write","volatile","atomic","ordering","address_space","alias_sets","size","alignment"]);
        blocks[blockId].append(instruction(x,v));
    }
    if("blocks" in root.object)foreach(v;root["blocks"].array)foreach(r;v["live_out"].array)blocks[unsigned(v["id"])].liveOut~=reference(x,r);
    foreach(b;f.blocks)if(!b.successors.length)foreach(v;root["outputs"].array)b.liveOut~=reference(x,v);
    foreach(v;root["outputs"].array)reference(x,v);
    foreach(b;f.blocks)if(b.successors.length>1&&(!b.instructions.length||b.instructions[$-1].effects.control!=ControlFlowKind.conditionalBranch))throw new ExchangeError("multiple successors need a conditional branch");
    foreach(d;root["dependencies"].array){known(d,["producer","consumer","kind","latency","distance","scheduler_only","latency_range"]);unsigned(d["latency"]);if(!(unsigned(d["producer"]) in ids)||!(unsigned(d["consumer"]) in ids))throw new ExchangeError("unknown dependency endpoint");}
    bool[uint] scheduled;
    foreach(s;root["schedule"].array){known(s,["id","cycle","slot","resources","additional_slots"]);auto id=unsigned(s["id"]);if(!(id in ids)||id in scheduled)throw new ExchangeError("invalid scheduled identity");scheduled[id]=true;unsigned(s["cycle"]);auto slot="slot" in s.object;if(slot !is null)unsigned(*slot);foreach(r;s["resources"].array)r.str;}
    if(scheduled.length&&scheduled.length!=ids.length)throw new ExchangeError("partial schedule");
    auto allocation="allocation" in root.object;
    if(allocation !is null){known(*allocation,["registers","spilled"]);bool[uint] assigned;foreach(r;(*allocation)["registers"].array){if(r.array.length!=2)throw new ExchangeError("invalid assignment");auto id=unsigned(r[0]);unsigned(r[1]);if(!(id in values)||id in assigned)throw new ExchangeError("invalid allocated value");assigned[id]=true;}foreach(v;(*allocation)["spilled"].array){auto id=unsigned(v);if(!(id in values)||id in assigned)throw new ExchangeError("invalid spilled value");assigned[id]=true;}}
    auto frame="spill_frame" in root.object;
    if(frame !is null){known(*frame,["size","slots"]);if(schemaVersion<2)throw new ExchangeError("spill frames need version 2");auto size=wideUnsigned((*frame)["size"]);bool[uint] slotted;ulong[2][] intervals;foreach(s;(*frame)["slots"].array){known(s,["value","class","offset","size","alignment"]);auto id=unsigned(s["value"]),bytes=unsigned(s["size"]),alignment=unsigned(s["alignment"]);auto offset=wideUnsigned(s["offset"]);if(!(id in values)||id in slotted||!s["class"].str.length||!bytes||!alignment||(alignment&(alignment-1))||offset%alignment||offset>size||bytes>size-offset)throw new ExchangeError("invalid spill slot");foreach(t;intervals)if(offset<t[1]&&t[0]<offset+bytes)throw new ExchangeError("overlapping spill slots");slotted[id]=true;intervals~=[offset,offset+bytes];}}
    recomputePredecessors(f);string error;if(!verify(f,error))throw new ExchangeError(error);validateContracts(x);return x;
}
/** Lossless return path. Versions 1 through 6 admit immediate/provenance edits;
 *  structural or effect changes require an explicit compiler adapter.
 */
string writeExchange(const ref RegionExchange x) {
    if(x.program.functions.length!=1)throw new ExchangeError("function-count edits require a compiler adapter");
    auto f=x.program.functions[0];string error;if(!verify(f,error))throw new ExchangeError(error);
    auto baseline=readExchange(toJSON(x.envelope));auto originalFunction=baseline.program.functions[0];
    if(f.blocks.length!=originalFunction.blocks.length||f.virtualRegisters!=originalFunction.virtualRegisters)throw new ExchangeError("structural edits require a compiler adapter");
    baseline.program.config.name=x.program.config.name;if(x.program.config!=baseline.program.config)throw new ExchangeError("machine-config edits require a target adapter");
    auto root=parseJSON(toJSON(x.envelope),JSONOptions.strictParsing);root["module"]=JSONValue(x.program.name);root["target"]=JSONValue(x.program.config.name);root["function"]=JSONValue(f.name);
    foreach(blockIndex,b;f.blocks){auto originalBlock=originalFunction.blocks[blockIndex];if(b.id!=originalBlock.id||b.successors!=originalBlock.successors||b.predecessors!=originalBlock.predecessors||b.liveOut!=originalBlock.liveOut||b.instructions.length!=originalBlock.instructions.length)throw new ExchangeError("CFG/dataflow edits require a compiler adapter");if("blocks" in root.object)root["blocks"][blockIndex]["name"]=JSONValue(b.label);else root["region"]=JSONValue(b.label);}
    foreach(k,contract;root["instructions"].array) {
        auto blockId=("blocks" in root.object)?unsigned(contract["block"]):0;auto b=findBlock(f,blockId);
        size_t index;foreach(n,v;root["instructions"].array[0..k])if(!("blocks" in root.object)||unsigned(v["block"])==blockId)++index;
        auto i=b.instructions[index];
        auto original=instruction(x,x.envelope["instructions"][k]);
        if(i.results!=original.results||i.effects!=original.effects||i.constraints!=original.constraints||i.operands.length!=original.operands.length)throw new ExchangeError("dataflow/effect edits require a compiler adapter");
        auto v=root["instructions"][k];v["opcode"]=JSONValue(i.opcode.name);v["origin"]=JSONValue(i.sourceName);size_t immediate;
        foreach(n,o;i.operands) {
            if(o.kind!=original.operands[n].kind)throw new ExchangeError("operand-kind edit requires a compiler adapter");
            if(o.kind==Operand.Kind.immediate)v["immediates"][immediate++]["integer"]=JSONValue(o.integer);
            else if(o!=original.operands[n])throw new ExchangeError("operand edit requires a compiler adapter");
        }
        if(i.opcode!=original.opcode)throw new ExchangeError("opcode edit requires scheduling/encoding adapters");
        root["instructions"][k]=v;
    }
    auto output=toJSON(root);readExchange(output);return output;
}
