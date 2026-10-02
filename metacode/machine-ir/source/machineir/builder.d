module machineir.builder;

import std.string : toLower;
import std.conv : to, parse;
import std.file : read;
import machineir.parser;
import machineir.isa;
import machineir.ir : MachineKind, ExecutionDomain, Endianness;

private uint uintv(Value* p, uint d=0, bool positive=false) {
    if (p is null) return d;
    auto s=p.asString();uint result;
    try {
        if(s.length>2&&s[0..2]=="0x") { auto digits=s[2..$];result=parse!uint(digits,16);if(digits.length)throw new Exception("trailing digits"); }
        else result=to!uint(s);
    } catch (Exception) { throw new ParseError("invalid unsigned integer: "~s,p.offset); }
    if(positive&&!result)throw new ParseError("expected a positive integer",p.offset);
    return result;
}
private string preserved(Value v) {
    return v.kind==ValueKind.object||v.kind==ValueKind.array||v.kind==ValueKind.bareList?v.toString():v.asString();
}
private ToolingContract contract(Field f) {
    ToolingContract c;c.name=f.name;
    if(f.value.kind==ValueKind.object)foreach(x;f.value.fields)c.values[x.name]=preserved(x.value);
    else c.values["value"]=preserved(f.value);
    return c;
}
private Endianness endian(string value) {
    if(value=="little")return Endianness.little;
    if(value=="big")return Endianness.big;
    if(value=="bi"||value=="bi_endian")return Endianness.bi;
    return Endianness.unknown;
}

MachineSpec buildMachineSpec(IsaDocument doc) {
    MachineSpec m;
    m.raw=doc.declarations;
    foreach (d; doc.declarations) {
        if (d.kind == "arch") {
            if(m.arch.length||!d.name.length)throw new ParseError("expected one named architecture",d.offset);
            m.arch=d.name; m.wordSize=uintv(findField(d.fields,"word_size"),0,true);
            m.addressSize=uintv(findField(d.fields,"addr_size"),0,true);
            auto e=scalar(findField(d.fields,"endian"));
            m.endian=endian(e);
            m.alignment=uintv(findField(d.fields,"align"));
            auto k=scalar(findField(d.fields,"kind"));
            foreach(value;[MachineKind.mpu,MachineKind.mcu,MachineKind.gpu,MachineKind.virtualMachine,MachineKind.bytecode,MachineKind.dsp,MachineKind.accelerator])
                if(k==to!string(value))m.kind=value;
        } else if (d.kind == "profile") {
            m.family=scalar(findField(d.fields,"family")); m.model=scalar(findField(d.fields,"model"));
            m.isaVersion=scalar(findField(d.fields,"version"));
            if (!m.wordSize) m.wordSize=uintv(findField(d.fields,"word_size"));
            if (!m.addressSize) m.addressSize=uintv(findField(d.fields,"address_size"));
            auto e=scalar(findField(d.fields,"endianness"));
            if (m.endian==Endianness.unknown)m.endian=endian(e);
            m.instructionEncoding=scalar(findField(d.fields,"instruction_encoding"));
            m.executionModel=scalar(findField(d.fields,"execution_model"));
            m.registerModel=scalar(findField(d.fields,"register_model"));
            m.extensions=strings(findField(d.fields,"extensions"));
            m.fixedRegisters=strings(findField(d.fields,"fixed_registers"));
            m.zeroRegister=scalar(findField(d.fields,"zero_register"));
            m.operationClasses=strings(findField(d.fields,"operation_classes"));
            m.encodingWidths=strings(findField(d.fields,"encoding_widths"));
        } else if (d.kind == "regclass") {
            if(d.name in m.registerClasses)throw new ParseError("duplicate register class: "~d.name,d.offset);
            RegisterClass rc; rc.name=d.name;
            foreach (f; d.fields) {
                PhysicalRegister pr; pr.name=f.name; pr.className=d.name;
                if (f.value.kind==ValueKind.object) {
                    pr.width=uintv(findField(f.value.fields,"width"),0,true);
                    pr.number=uintv(findField(f.value.fields,"index"));
                }
                rc.registers ~= pr;
            }
            m.registerClasses[rc.name]=rc;
        } else if (d.kind == "alias") {
            if(d.name in m.aliases)throw new ParseError("duplicate register alias: "~d.name,d.offset);
            m.aliases[d.name]=scalar(findField(d.fields,"value"));
        } else if (d.kind == "encoding") {
            if(d.name in m.encodings)throw new ParseError("duplicate encoding: "~d.name,d.offset);
            InstructionEncoding enc; enc.name=d.name; enc.raw=d.name;
            auto width=findField(d.fields,"width");auto spelling=scalar(width);
            if(spelling!="text"&&spelling!="stream"&&spelling!="variable"&&spelling!="unknown")enc.width=uintv(width,0,true);
            enc.base=scalar(findField(d.fields,"base"));
            foreach (f; d.fields) if (f.name!="width" && f.name!="base")
                enc.fields ~= EncodingField(f.name,preserved(f.value));
            m.encodings[enc.name]=enc;
        } else if (d.kind == "op") {
            if(d.name in m.instructions)throw new ParseError("duplicate instruction: "~d.name,d.offset);
            InstructionSpec op; op.name=d.name;
            op.className=scalar(findField(d.fields,"class"));
            op.encoding=scalar(findField(d.fields,"encoding"));
            op.syntax=scalar(findField(d.fields,"syntax"));
            op.semantics=scalar(findField(d.fields,"semantics"));
            op.operands=strings(findField(d.fields,"operands"));
            op.rawClass=op.className;
            auto t=findField(d.fields,"tooling");
            if (t !is null && t.kind==ValueKind.object) {
                foreach (tf; t.fields) {
                    op.tooling ~= contract(tf);
                }
            }
            m.instructions[op.name]=op;
        } else if (d.kind == "compiler") {
            foreach (f; d.fields) {
                m.compilerContracts ~= contract(f);
            }
        } else if(d.kind=="tooling") {
            foreach(f;d.fields)m.toolingContracts~=contract(f);
            auto bt=findField(d.fields,"bin2bin");auto domain=nestedScalar(bt,"execution_domain");
            if(domain=="native_machine_code")m.executionDomain=ExecutionDomain.nativeMachineCode;
            else if(domain=="bytecode")m.executionDomain=ExecutionDomain.bytecode;
            else if(domain=="virtual_isa")m.executionDomain=ExecutionDomain.virtualISA;
            else if(domain=="shader_ir")m.executionDomain=ExecutionDomain.shaderIR;
            else if(domain=="interpreter")m.executionDomain=ExecutionDomain.interpreter;
            else if(domain=="jit")m.executionDomain=ExecutionDomain.jit;
        }
    }
    if(!m.arch.length)throw new ParseError("ISA contains no architecture",0);
    return m;
}


MachineSpec loadISA(string path) {
    return buildMachineSpec(parseISA(cast(string)read(path)));
}
