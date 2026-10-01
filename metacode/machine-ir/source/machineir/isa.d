module machineir.isa;

import machineir.parser : Value, ValueKind, Field, Declaration, IsaDocument;
import machineir.ir : MachineKind, ExecutionDomain, Endianness;

struct PhysicalRegister {
    string name;
    uint number;
    uint width;
    string className;
}

struct RegisterClass {
    string name;
    PhysicalRegister[] registers;
    string allocationPolicy;
    bool isVector;
}

struct EncodingField {
    string name;
    string value;
}

struct InstructionEncoding {
    string name;
    uint width;
    EncodingField[] fields;
    string base;
    string raw;
}

struct ToolingContract {
    string name;
    string[string] values;
}

struct InstructionSpec {
    string name;
    string className;
    string encoding;
    string syntax;
    string semantics;
    string[] operands;
    ToolingContract[] tooling;
    bool pseudo;
    string rawClass;
}

struct MachineSpec {
    string arch;
    string family;
    string model;
    string version;
    MachineKind kind;
    ExecutionDomain executionDomain;
    uint wordSize;
    uint addressSize;
    Endianness endian;
    uint alignment;
    string instructionEncoding;
    string executionModel;
    string registerModel;
    string[] extensions;
    string[] fixedRegisters;
    string zeroRegister;
    string[] operationClasses;
    string[] encodingWidths;

    RegisterClass[string] registerClasses;
    InstructionEncoding[string] encodings;
    InstructionSpec[string] instructions;
    string[string] aliases;
    ToolingContract[] compilerContracts;

    // Retains declarations/fields that MachineIR does not yet normalize.
    Declaration[] raw;
}

string scalar(Value* p) {
    return p is null ? "" : p.asString();
}
Value* findField(ref Field[] fs, string name) {
    foreach (ref f; fs) if (f.name == name) return &f.value;
    return null;
}
string[] strings(Value* p) {
    string[] r;
    if (p is null) return r;
    if (p.kind == ValueKind.array || p.kind == ValueKind.bareList)
        foreach (x; p.items) r ~= x.asString();
    else if (p.asString().length) r ~= p.asString();
    return r;
}
string nestedScalar(Value* object, string name) {
    if (object is null || object.kind != ValueKind.object) return "";
    foreach (f; object.fields) if (f.name == name) return f.value.asString();
    return "";
}
