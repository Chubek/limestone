module machineir.builder;

import std.string : toLower;
import std.algorithm : canFind;
import std.conv : to;
import std.file : read;
import machineir.parser;
import machineir.isa;
import machineir.ir : MachineKind, ExecutionDomain, Endianness;

private string lower(string s) { return s.toLower; }

private MachineKind inferKind(string arch, string family, string model, string exec) {
    auto s=(arch~" "~family~" "~model~" "~exec).toLower;
    if (s.canFind("amdgpu") || s.canFind("nvptx") || s.canFind("gpu")) return MachineKind.gpu;
    if (s.canFind("spir-v") || s.canFind("spirv") || s.canFind("wasm") ||
        s.canFind("virtual_isa") || s.canFind("bytecode") || s.canFind("virtual"))
        return MachineKind.virtualMachine;
    if (s.canFind("avr") || s.canFind("mcu")) return MachineKind.mcu;
    if (s.canFind("dsp")) return MachineKind.dsp;
    return MachineKind.mpu;
}

private ExecutionDomain inferDomain(string enc, string exec, string arch) {
    auto s=(enc~" "~exec~" "~arch).toLower;
    if (s.canFind("bytecode") || s.canFind("wasm")) return ExecutionDomain.bytecode;
    if (s.canFind("virtual") || s.canFind("ptx") || s.canFind("spir")) return ExecutionDomain.virtualISA;
    if (s.canFind("shader")) return ExecutionDomain.shaderIR;
    if (s.canFind("jit")) return ExecutionDomain.jit;
    return ExecutionDomain.nativeMachineCode;
}

private uint uintv(Value* p, uint d=0) {
    if (p is null) return d;
    try { return to!uint(p.asString()); } catch (Exception) {}
    return d;
}

MachineSpec buildMachineSpec(IsaDocument doc) {
    MachineSpec m;
    m.raw=doc.declarations;
    foreach (d; doc.declarations) {
        if (d.kind == "arch") {
            m.arch=d.name; m.wordSize=uintv(findField(d.fields,"word_size"));
            m.addressSize=uintv(findField(d.fields,"addr_size"));
            auto e=scalar(findField(d.fields,"endian"));
            m.endian = e=="little" ? Endianness.little : e=="big" ? Endianness.big : Endianness.unknown;
            m.alignment=uintv(findField(d.fields,"align"));
        } else if (d.kind == "profile") {
            m.family=scalar(findField(d.fields,"family")); m.model=scalar(findField(d.fields,"model"));
            m.version=scalar(findField(d.fields,"version"));
            if (!m.wordSize) m.wordSize=uintv(findField(d.fields,"word_size"));
            if (!m.addressSize) m.addressSize=uintv(findField(d.fields,"address_size"));
            auto e=scalar(findField(d.fields,"endianness"));
            if (m.endian==Endianness.unknown)
                m.endian=e=="little"?Endianness.little:e=="big"?Endianness.big:Endianness.unknown;
            m.instructionEncoding=scalar(findField(d.fields,"instruction_encoding"));
            m.executionModel=scalar(findField(d.fields,"execution_model"));
            m.registerModel=scalar(findField(d.fields,"register_model"));
            m.extensions=strings(findField(d.fields,"extensions"));
            m.fixedRegisters=strings(findField(d.fields,"fixed_registers"));
            m.zeroRegister=scalar(findField(d.fields,"zero_register"));
            m.operationClasses=strings(findField(d.fields,"operation_classes"));
            m.encodingWidths=strings(findField(d.fields,"encoding_widths"));
        } else if (d.kind == "regclass") {
            RegisterClass rc; rc.name=d.name;
            foreach (f; d.fields) {
                PhysicalRegister pr; pr.name=f.name; pr.className=d.name;
                if (f.value.kind==ValueKind.object) {
                    pr.width=uintv(findField(f.value.fields,"width"));
                    auto ix=scalar(findField(f.value.fields,"index"));
                    try { pr.number=to!uint(ix); } catch(Exception) {}
                }
                rc.registers ~= pr;
            }
            m.registerClasses[rc.name]=rc;
        } else if (d.kind == "alias") {
            m.aliases[d.name]=scalar(findField(d.fields,"value"));
        } else if (d.kind == "encoding") {
            InstructionEncoding enc; enc.name=d.name; enc.raw=d.name;
            enc.width=uintv(findField(d.fields,"width"));
            enc.base=scalar(findField(d.fields,"base"));
            foreach (f; d.fields) if (f.name!="width" && f.name!="base")
                enc.fields ~= EncodingField(f.name,f.value.asString());
            m.encodings[enc.name]=enc;
        } else if (d.kind == "op") {
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
                    ToolingContract c; c.name=tf.name;
                    if (tf.value.kind==ValueKind.object)
                        foreach (x; tf.value.fields) c.values[x.name]=x.value.asString();
                    else c.values["value"]=tf.value.asString();
                    op.tooling ~= c;
                }
                auto bt=findField(t.fields,"binary_translation");
                if (bt !is null) {}
            }
            m.instructions[op.name]=op;
        } else if (d.kind == "compiler") {
            foreach (f; d.fields) {
                ToolingContract c; c.name=f.name;
                if (f.value.kind==ValueKind.object)
                    foreach (x; f.value.fields) c.values[x.name]=x.value.asString();
                m.compilerContracts ~= c;
            }
        }
    }
    m.kind=inferKind(m.arch,m.family,m.model,m.executionModel);
    m.executionDomain=inferDomain(m.instructionEncoding,m.executionModel,m.arch);
    return m;
}


MachineSpec loadISA(string path) {
    return buildMachineSpec(parseISA(cast(string)read(path)));
}
