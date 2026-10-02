module machineir.serialize;

import std.array : appender;
import machineir.ir;
import machineir.isa;
import std.algorithm : sort;
import std.conv : to;

string dump(const(MachineSpec) m) {
    auto a=appender!string();
    a.put("machine "); a.put(m.arch); a.put(" {\n");
    a.put("  family = "); a.put(m.family); a.put("\n");
    a.put("  model = "); a.put(m.model); a.put("\n");
    a.put("  kind = "); a.put(kindName(m.kind)); a.put("\n");
    a.put("  word_size = "); a.put(toString(m.wordSize)); a.put("\n");
    a.put("  address_size = "); a.put(toString(m.addressSize)); a.put("\n");
    a.put("  register_classes = "); a.put(toString(m.registerClasses.length)); a.put("\n");
    a.put("  encodings = "); a.put(toString(m.encodings.length)); a.put("\n");
    a.put("  instructions = "); a.put(toString(m.instructions.length)); a.put("\n");
    a.put("}\n");
    return a.data;
}
private string kindName(MachineKind k) { return toString(k); }
private string toString(T)(T x) { import std.conv : to; return to!string(x); }

private string operand(const(Operand) o) {
    final switch(o.kind) {
    case Operand.Kind.reg:
        return (o.registerRef.kind==RegKind.virtualRegister?"%":"$")~o.registerRef.name~":"~o.registerRef.className~":"~to!string(o.registerRef.width);
    case Operand.Kind.immediate: return "#"~to!string(o.integer);
    case Operand.Kind.block: return "block "~to!string(o.blockId);
    case Operand.Kind.symbol: return "symbol "~o.text;
    case Operand.Kind.literal: return "literal "~o.text;
    case Operand.Kind.memory:
        return "memory("~to!string(o.memory.addressSpace)~","~to!string(o.memory.width)~","~o.memory.addressExpression~")";
    }
}
/** Deterministic diagnostic serialization preserving operand and effect classes. */
string dump(MachineFunction f) {
    if(f is null)return "<null function>\n";
    auto output=appender!string();output.put("function "~f.name~" {\n");
    auto blocks=f.blocks.dup;blocks.sort!((a,b)=>a.id<b.id);
    foreach(b;blocks) {
        output.put("  block "~to!string(b.id)~" "~b.label~" {\n");
        foreach(i;b.instructions) {
            output.put("    ");foreach(k,r;i.results){if(k)output.put(", ");output.put(operand(r));}
            if(i.results.length)output.put(" = ");output.put(i.opcode.name);
            foreach(o;i.operands)output.put(" "~operand(o));
            output.put(" ; memory="~to!string(i.effects.memory)~" control="~to!string(i.effects.control));
            if(i.effects.mayTrap)output.put(" may-trap");if(i.effects.atomic)output.put(" atomic");
            if(i.effects.serializing)output.put(" serializing");if(i.effects.privileged)output.put(" privileged");
            foreach(s;i.effects.uses)output.put(" use="~s);foreach(s;i.effects.defs)output.put(" def="~s);
            foreach(s;i.effects.implicitUses)output.put(" implicit-use="~s);foreach(s;i.effects.implicitDefs)output.put(" implicit-def="~s);
            foreach(s;i.effects.flagsRead)output.put(" flag-read="~s);foreach(s;i.effects.flagsWritten)output.put(" flag-write="~s);
            foreach(s;i.constraints)output.put(" constraint="~s);
            if(i.sourceName.length)output.put(" source="~i.sourceName);
            if(i.semantic.length)output.put(" semantics="~i.semantic);output.put("\n");
        }
        auto successors=b.successors.dup;successors.sort;
        foreach(r;b.liveOut)output.put("    live-out "~r.name~"\n");
        foreach(s;successors)output.put("    successor "~to!string(s)~"\n");output.put("  }\n");
    }
    output.put("}\n");return output.data;
}
