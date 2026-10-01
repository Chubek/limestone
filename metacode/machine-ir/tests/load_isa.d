module load_isa;

import std.file : read, dirEntries, SpanMode;
import std.path : buildPath;
import std.stdio : writeln;
import std.string : endsWith, strip, startsWith;
import machineir;

void main() {
    auto root = "../infobank/isa";
    size_t count;
    foreach (e; dirEntries(root, SpanMode.shallow)) {
        if (!e.name.endsWith(".isa")) continue;
        auto text = cast(string)read(e.name);
        if (text.strip.startsWith("legacy")) continue;
        auto doc = parseISA(text);
        auto spec = buildMachineSpec(doc);
        assert(spec.arch.length, e.name);
        ++count;
        writeln(spec.arch, ": ", spec.instructions.length, " instructions, ",
                spec.registerClasses.length, " register classes");
    }
    assert(count > 0);
}
