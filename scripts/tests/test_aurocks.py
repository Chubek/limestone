#!/usr/bin/env python3
"""Generator integration tests: generated code is compiled and executed.

Run: python3 scripts/tests/test_aurocks.py
Optional toolchains: CC, CXX, GO, DC, JAVAC, JAVA. Unavailable targets skip;
generation, ASDL mapping and Python behavioral regressions always run.
"""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / "scripts/aurocks.pl"
FIXTURE = Path(__file__).parent / "fixtures/aurocks.g"


def run(args, cwd=None, input=None, env=None):
    p = subprocess.run([str(x) for x in args], cwd=cwd, input=input,
                       text=True, capture_output=True, env=env, timeout=120)
    if p.returncode:
        raise AssertionError(f"{args}: exit {p.returncode}\n{p.stdout}\n{p.stderr}")
    return p.stdout


def load(path):
    name = "aurocks_test_" + path.stem
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


class AurocksTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="aurocks-", dir="/tmp/opencode" if Path("/tmp/opencode").exists() else None)
        cls.work = Path(cls.tmp.name)
        cls.py = cls.work / "reference.py"
        run(["perl", GEN, "-Tpython", "-Mvisitor", FIXTURE, "-o", cls.py])
        cls.reference = load(cls.py)
        cls.result = cls.reference.parse_Expr("12 + 34+56")
        cls.binary = cls.reference.events_to_binary(cls.result.events)
        (cls.work / "reference.bin").write_bytes(cls.binary)
        (cls.work / "input.txt").write_text("12 + 34+56")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def generate(self, target, dest, grammar=FIXTURE, modes="visitor"):
        args = ["perl", GEN, "-T" + target, "-o", dest]
        if modes:
            args += ["-M", modes]
        run(args + [grammar])

    def tool(self, name, default):
        value = os.environ.get(name) or shutil.which(default)
        if not value:
            self.skipTest(f"{default} unavailable")
        return value

    def check_binary(self, name):
        self.assertEqual((self.work / name).read_bytes(), self.binary)

    def test_python_ast_callbacks_visitors(self):
        p = self.reference
        calls, seen = [], []
        r = p.parse_Expr("12 + 34+56", {"on_number": lambda n: calls.append(n.ast.value)}, seen.append)
        self.assertEqual(calls, [12, 34, 56])
        self.assertEqual(r.ast, p.Add(p.Add(p.Wrap(p.Integer(12)), p.Integer(34)), p.Integer(56)))
        self.assertEqual(seen, r.events)
        self.assertIsNone(p.parse_Expr("12+x", {"on_number": lambda n: calls.append(0)}, seen.append))
        self.assertEqual(calls, [12, 34, 56])
        self.assertEqual(seen, r.events)
        visited = []
        p.visit(r.root, lambda n: visited.append(n.kind))
        self.assertEqual(visited[-1], "Add")
        class Sum(p.ASDLVisitor):
            def visit_Integer(self, n):
                return n.value
            def visit_Wrap(self, n):
                return self.visit(n.number)
            def visit_Add(self, n):
                return self.visit(n.left) + self.visit(n.right)
        self.assertEqual(Sum().visit(r.ast), 102)
        self.assertEqual(p.parse_file(self.work / "input.txt").ast, r.ast)
        self.assertEqual(p.events_from_binary(self.binary), r.events)
        replayed = []
        p.replay(r.events, replayed.append)
        self.assertEqual(replayed, r.events)
        for bad in [b"", self.binary[:11], self.binary[:-1], self.binary + b"x", b"X" + self.binary[1:]]:
            with self.assertRaises(ValueError):
                p.events_from_binary(bad)

    def test_grammar_and_cli(self):
        self.assertEqual(set(run(["perl", GEN, "--list-targets"]).split()), {"c", "cpp", "d", "python", "ruby", "java", "go", "m4"})
        default = run(["perl", GEN, FIXTURE])
        self.assertIn("#include <stdint.h>", default)
        forced = self.work / "forced.g"
        forced.write_text("%target python\n" + FIXTURE.read_text())
        out = self.work / "forced.py"
        self.generate("go", out, forced, modes="")
        self.assertIsNotNone(load(out).parse_Expr("1+2"))
        for target, ext in [("c++", "cpp"), ("java", "java"), ("go", "go"), ("d", "d"), ("ruby", "rb"), ("m4", "m4")]:
            self.generate(target, self.work / ("generated." + ext))
        schema = run(["perl", GEN, "--emit-asdl", FIXTURE])
        inferred = self.work / "inferred.asdl"
        inferred.write_text(schema)
        m = json.loads(run(["perl", ROOT / "scripts/ASDL.pl", "--asdl-json", inferred]))
        self.assertEqual(m["types"]["Expr"]["cons"][0]["tag"], "Expr_1")
        self.assertEqual(json.loads(run(["python3", ROOT / "scripts/ASDL.py", "--asdl-json", inferred])), m)

    def test_nullable_indirect_recursion_and_regex(self):
        grammars = [
            ("S: A 'b'; A: S 'a' | %empty;", ["b", "bab", "babab"], ["", "ba", "bb"]),
            ("S: A A 'x'; A: A | %empty;", ["x"], ["", "xx"]),
            ("S: A 'c'; A: 'a' | 'a' 'b';", ["ac", "abc"], ["ab", "acc"]),
            (r"S: /[[:alpha:]_][\w]{1,3}/ ':' /\d+/ '$';", ["ab:12$", "_abc:9$"], ["a:1$", "abcdx:2$"]),
            (r"S: /(a|b)+c?$/;", ["aa", "abc", "bbbc"], ["", "acx"]),
            (r"S: /[^\n]+/;", ["abc", "a'b", "é"], ["", "a\nb"]),
        ]
        for i, (rules, valid, invalid) in enumerate(grammars):
            grammar = self.work / f"regression{i}.g"
            grammar.write_text("%start S\n%%\n" + rules + "\n%%\n")
            out = self.work / f"regression{i}.py"
            self.generate("python", out, grammar, modes="")
            p = load(out)
            for text in valid:
                self.assertIsNotNone(p.parse_S(text), (rules, text))
            for text in invalid:
                self.assertIsNone(p.parse_S(text), (rules, text))

    def test_semantic_actions_opt_in(self):
        grammar = self.work / "actions.g"
        grammar.write_text("%start S\n%%\nS: Number '+' Number { $$ = $1 + $3 };\nNumber: /[0-9]+/ { $$ = int($TOKEN) };\n%%\n")
        out = self.work / "actions.py"
        self.generate("python", out, grammar, modes="")
        p = load(out)
        self.assertIsInstance(p.parse_S("2+3").value, p.Node)
        self.assertEqual(p.parse_S("2+3").root, p.parse_S("2+3").root)
        grammar.write_text("%actions ON\n" + grammar.read_text())
        self.generate("python", out, grammar, modes="")
        self.assertEqual(load(out).parse_S("2+3").value, 5)

    def test_asdl_lowercase_types_sequences_optional_products(self):
        grammar = self.work / "schema.g"
        grammar.write_text("%start Pair\n%mode ast\n%%\nPair: Number ',' Number;\nNumber: /[0-9]+/;\n%%\n")
        schema = grammar.with_suffix(".asdl")
        schema.write_text("module Model { pair = PairNode(number* values, string? note) [tag Pair]\nnumber = Num(int value) [tag Number] }")
        out = self.work / "schema.py"
        self.generate("python", out, grammar, modes="visitor")
        p = load(out)
        self.assertEqual(p.parse_Pair("1,2").ast, p.PairNode([p.Num(1), p.Num(2)], ","))
        schema.write_text("module Model { pair = (number* values) [tag Pair]\nnumber = Num(int value) [tag Number] }")
        self.generate("python", out, grammar, modes="visitor")
        p = load(out)
        self.assertEqual(p.parse_Pair("1,2").ast.values, [p.Num(1), p.Num(2)])

    def test_native_actions_committed_and_custom_entrypoint(self):
        for lang in ("c", "cpp", "ruby", "go", "d"):
            grammar = self.work / ("actions_" + lang + ".g")
            prologue = {"c": "static int action_count;", "cpp": "static int action_count;", "ruby": "$action_count = 0", "go": "var actionCount int", "d": "int actionCount;"}[lang]
            action = {"c": "action_count++; $$ = $1;", "cpp": "action_count++; $$ = $1;", "ruby": "$action_count += 1; $$ = $1", "go": "actionCount++; $$ = $1", "d": "actionCount++; $$ = $1;"}[lang]
            grammar.write_text(f"%{{\n{prologue}\n%}}\n%actions ON\n%start S\n%%\nS: /[0-9]+/ {{ {action} }};\n%%\n")
            ext = {"c": "c", "cpp": "cpp", "ruby": "rb", "go": "go", "d": "d"}[lang]
            out = self.work / ("action_parser." + ext)
            run(["perl", GEN, "-T" + lang, "-e", "read_value", grammar, "-o", out])
            if lang in ("c", "cpp"):
                driver = self.work / ("action_driver." + ext)
                driver.write_text(f'#include "{out.name}"\n#include <assert.h>\nint main(void) {{ assert(!read_value("42x",NULL) && action_count==0); AurocksResult *r=read_value("42",NULL); assert(r && action_count==1 && !strcmp((char*)r->value,"42")); aurocks_free(r); }}\n')
                exe = self.work / ("actions-" + lang)
                cc = self.tool("CC" if lang == "c" else "CXX", "gcc" if lang == "c" else "g++")
                run([cc, "-std=c99" if lang == "c" else "-std=c++20", "-Wall", "-Wextra", "-Werror", driver, "-o", exe])
                run([exe])
            elif lang == "ruby":
                run([self.tool("RUBY", "ruby"), "-r", out, "-"], input='raise unless read_value("42x").nil? && $action_count==0\nr=read_value("42"); raise unless r.value=="42" && $action_count==1\n')
            elif lang == "go":
                driver = self.work / "actions_test.go"
                driver.write_text('package aurocks\nimport "testing"\nfunc TestActions(t *testing.T) { if read_value("42x",nil,nil)!=nil || actionCount!=0 { t.Fatal("failed actions") }; r:=read_value("42",nil,nil); if r.Value.(string)!="42" || actionCount!=1 { t.Fatal("actions") } }\n')
                go = self.tool("GO", "go")
                env = dict(os.environ)
                if "GO" not in os.environ and Path("/usr/lib/go/src/encoding/json").exists():
                    go = "/usr/lib/go/bin/go"
                    env.update(GOROOT="/usr/lib/go", GOTOOLCHAIN="local")
                run([go, "test", "-vet=off", out, driver], env=env)
            else:
                driver = self.work / "actions_driver.d"
                driver.write_text('module actions_driver;\nimport aurocks;\nvoid main() { assert(read_value("42x") is null && actionCount==0); auto r=read_value("42"); assert(r !is null && (cast(Node)r.value).text=="42" && actionCount==1); }\n')
                exe = self.work / "actions-d"
                run([self.tool("DC", "ldc2"), out, driver, "-of=" + str(exe)])
                run([exe])

    def test_real_isa_grammar(self):
        out = self.work / "isa.py"
        self.generate("python", out, ROOT / "parsers/isa.g", modes="")
        p = load(out)
        for text in ("", "arch Example {}", 'arch Example { endian = "little"; }', "profile {}"):
            self.assertIsNotNone(p.parse_Document(text), text)
        self.assertIsNone(p.parse_Document("arch"))
        out = self.work / "isa.c"
        self.generate("c", out, ROOT / "parsers/isa.g", modes="")
        run([self.tool("CC", "gcc"), "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", "-fsyntax-only", out])

    def test_c_and_cpp_runtime(self):
        driver = r'''
#include "parser.c"
#include <assert.h>
static int calls, observed, visited;
static void callback(AurocksNode *n,void *u) { (void)u; assert(aurocks_field_int(n,"value")>0); calls++; }
static void listener(const AurocksEvent *e,void *u) { (void)u; assert(e->kind>=1 && e->kind<=3); observed++; }
static void visitor(AurocksNode *n,void *u) { (void)n; (void)u; visited++; }
int main(void) {
    const AurocksCallbackEntry entries[]={{"on_number",callback}};
    AurocksOptions options={entries,1,listener,NULL};
    AurocksResult *r=parse_Expr("12 + 34+56",&options); assert(r && calls==3 && observed==17);
    const AurocksField *right=aurocks_field(r->root,"right"); assert(right && right->count==1 && aurocks_field_int(right->items[0],"value")==56);
    assert(!parse_Expr("1+x",&options) && calls==3 && observed==17);
    aurocks_visit(r->root,visitor,NULL); assert(visited==11);
    visited=0; AurocksVisitor av; memset(&av,0,sizeof av); av.generic=visitor; aurocks_visit_ast(r->root,&av); assert(visited==6);
    FILE *f=fopen("reference.bin","rb"); AurocksEvent *events=NULL; size_t count=0;
    assert(aurocks_events_read(f,&events,&count) && count==17); fclose(f);
    aurocks_replay(events,count,listener,NULL); assert(observed==34);
    f=fopen("c.bin","wb"); assert(aurocks_events_write(f,r->events,r->event_count)); fclose(f);
    aurocks_events_free(events,count); aurocks_free(r);
    r=parse_file("input.txt",NULL); assert(r); aurocks_free(r);
    return 0;
}
'''
        for target, tool, flags in [("c", self.tool("CC", "gcc"), ["-std=c99"]), ("cpp", self.tool("CXX", "g++"), ["-std=c++20"])]:
            self.generate(target, self.work / "parser.c")
            path = self.work / ("driver.c" if target == "c" else "driver.cpp")
            path.write_text(driver)
            exe = self.work / ("run-" + target)
            run([tool, *flags, "-Wall", "-Wextra", "-Werror", "-pedantic", path, "-o", exe])
            run([exe], cwd=self.work)
            self.check_binary("c.bin")

    def test_ruby_runtime(self):
        ruby = self.tool("RUBY", "ruby")
        self.generate("ruby", self.work / "parser.rb")
        run([ruby, "-r", self.work / "parser.rb", "-"], cwd=self.work, input=r'''
calls, seen = [], []
r=parse_Expr('12 + 34+56', {'on_number'=>->(n){calls << n.fields['value']}}, ->(e){seen << e})
raise unless calls == [12,34,56] && seen == r.events && r.root.kind=='Add'
raise unless parse_Expr('1+x', {'on_number'=>->(n){calls << 0}}).nil? && calls.size==3
raise unless r.root.fields['right'].fields['value']==56
raise unless parse_file('input.txt').root.kind=='Add'
visited=[]; visit(r.root, ->(n){visited << n}); raise unless visited.size==11
v=Class.new(AurocksVisitor){attr_reader :count; def initialize; @count=0; end; def generic_visit(n); @count+=1; end}.new
v.visit(r.root); raise unless v.count==6
raise unless events_from_binary(File.binread('reference.bin'))==r.events
File.binwrite('ruby.bin',events_to_binary(r.events))
replayed=[]; replay(r.events, ->(e){replayed << e}); raise unless replayed==r.events
''')
        self.check_binary("ruby.bin")

    def test_go_runtime(self):
        go = self.tool("GO", "go")
        env = dict(os.environ)
        if "GO" not in os.environ and Path("/usr/lib/go/src/encoding/json").exists():
            go = "/usr/lib/go/bin/go"
            env.update(GOROOT="/usr/lib/go", GOTOOLCHAIN="local")
        self.generate("go", self.work / "parser.go")
        driver = self.work / "parser_test.go"
        driver.write_text(r'''
package aurocks
import("testing";"os";"bytes")
func TestRuntime(t *testing.T) {
    calls,seen:=0,0
    cb:=Callbacks{"on_number":func(n *Node){ calls++; if n.Fields["value"].(int64)<=0 { t.Fatal("field") } }}
    listener:=ListenerFuncs{OnEnter:func(Event){seen++},OnTerminal:func(Event){seen++},OnExit:func(Event){seen++}}
    r:=Parse("12 + 34+56",cb,listener)
    if r==nil || calls!=3 || seen!=17 || r.Root.Fields["right"].(*Node).Fields["value"].(int64)!=56 { t.Fatal("parse") }
    if Parse("1+x",cb,listener)!=nil || calls!=3 || seen!=17 { t.Fatal("failed callbacks") }
    count:=0; Visit(r.Root,func(*Node){count++}); if count!=11 { t.Fatal("visitor") }
    count=0; VisitAST(r.Root,ASTVisitor{Generic:func(*Node){count++}}); if count!=6 { t.Fatal("AST visitor") }
    data,_:=os.ReadFile("reference.bin"); ev,e:=EventsFromBinary(data); if e!=nil || len(ev)!=17 || !bytes.Equal(EventsToBinary(ev),data) { t.Fatal("deserialize",e) }
    Replay(ev,listener); if seen!=34 { t.Fatal("replay") }; os.WriteFile("go.bin",EventsToBinary(r.Events),0600)
    if rr,e:=ParseFile("input.txt",nil,nil); rr==nil || e!=nil { t.Fatal("file") }
    if _,e:=EventsFromBinary(data[:len(data)-1]); e==nil { t.Fatal("truncated") }
}
''')
        run([go, "test", "-vet=off", self.work / "parser.go", driver], cwd=self.work, env=env)
        self.check_binary("go.bin")

    def test_d_runtime(self):
        dc = self.tool("DC", "ldc2")
        self.generate("d", self.work / "parser.d")
        driver = self.work / "driver.d"
        driver.write_text(r'''
module driver;
import aurocks;
import std.file : read, write;
class CountListener : ListenerBase { int count; override void enter(Event) {count++;} override void terminal(Event) {count++;} override void exit(Event) {count++;} }
class CountVisitor : ASTVisitor { int count; override void genericVisit(Node n) {count++;} }
void main() {
    int calls; Callbacks cb; cb["on_number"]=(Node n){assert(n.fieldInt("value")>0);calls++;}; auto listener=new CountListener;
    auto r=parse_Expr("12 + 34+56",cb,listener); assert(r !is null && calls==3 && listener.count==17);
    assert(r.root.fields["right"][0].fieldInt("value")==56);
    assert(parse_Expr("1+x",cb,listener) is null && calls==3 && listener.count==17);
    int count; visit(r.root,(Node n){count++;}); assert(count==11);
    auto v=new CountVisitor; v.visit(r.root); assert(v.count==6);
    auto data=cast(ubyte[])read("reference.bin"); auto ev=eventsFromBinary(data); assert(ev.length==17 && eventsToBinary(ev)==data);
    replay(ev,listener); assert(listener.count==34); write("d.bin",eventsToBinary(r.events));
    assert(parseFile("input.txt") !is null);
    bool rejected; try { eventsFromBinary(data[0..$-1]); } catch(Exception) { rejected=true; } assert(rejected);
}
''')
        exe = self.work / "run-d"
        run([dc, self.work / "parser.d", driver, "-of=" + str(exe)])
        run([exe], cwd=self.work)
        self.check_binary("d.bin")

    def test_java_runtime(self):
        javac = self.tool("JAVAC", "javac")
        java = self.tool("JAVA", "java")
        self.generate("java", self.work / "AurocksExpr.java")
        driver = self.work / "Driver.java"
        driver.write_text(r'''
import java.util.*;
import java.nio.file.*;
public class Driver {
    static int calls,seen,visited;
    public static void main(String[] args) throws Exception {
        var listener=new AurocksExpr.Listener() { public void enter(AurocksExpr.Event e) {seen++;} public void terminal(AurocksExpr.Event e) {seen++;} public void exit(AurocksExpr.Event e) {seen++;} };
        Map<String,java.util.function.Consumer<AurocksExpr.Node>> cb=Map.of("on_number",n->{calls++;assert (Long)n.fields.get("value")>0;});
        var r=AurocksExpr.parse_Expr("12 + 34+56",cb,listener); assert r!=null && calls==3 && seen==17;
        assert (Long)((AurocksExpr.Node)r.root().fields.get("right")).fields.get("value")==56;
        assert AurocksExpr.parse_Expr("1+x",cb,listener)==null && calls==3 && seen==17;
        AurocksExpr.visit(r.root(),new AurocksExpr.Visitor(){public void visit(AurocksExpr.Node n){visited++;}}); assert visited==11;
        visited=0; AurocksExpr.visitAST(r.root(),new AurocksExpr.ASTVisitor(){public void genericVisit(AurocksExpr.Node n){visited++;}}); assert visited==6;
        byte[] data=Files.readAllBytes(Path.of("reference.bin")); var ev=AurocksExpr.eventsFromBinary(data); assert ev.size()==17 && Arrays.equals(data,AurocksExpr.eventsToBinary(ev));
        AurocksExpr.replay(ev,listener); assert seen==34; Files.write(Path.of("java.bin"),AurocksExpr.eventsToBinary(r.events()));
        assert AurocksExpr.parseFile(Path.of("input.txt"),null,null)!=null;
    }
}
''')
        run([javac, self.work / "AurocksExpr.java", driver])
        run([java, "-ea", "-cp", self.work, "Driver"], cwd=self.work)
        self.check_binary("java.bin")

    def test_m4_runtime(self):
        m4 = self.tool("M4", "m4")
        out = self.work / "parser.m4"
        self.generate("m4", out)
        p = json.loads(run([m4, out, "-"], input="AUROCKS_PARSE({{{12 + 34+56}}})\n"))
        self.assertEqual(p["root"]["kind"], "Add")
        self.assertEqual(len(p["events"]), 17)
        result = run([m4, out, "-"], input="AUROCKS_EVENTS_FILE({{{12 + 34+56}}}, {{{" + str(self.work / "m4.bin") + "}}})\n")
        self.assertEqual(json.loads(result)["root"]["end"], 10)
        self.check_binary("m4.bin")
        result = run([m4, out, "-"], input="AUROCKS_REPLAY_FILE({{{" + str(self.work / "reference.bin") + "}}})\n")
        self.assertEqual(len(json.loads(result)), 17)
        macros = "define({{{AUROCKS_CB_on_number}}}, {{{[$1]}}})dnl\nAUROCKS_PARSE({{{1+2}}})\n"
        result = run([m4, out, "-"], input=macros)
        self.assertTrue(result.startswith("[31][32]"), result[:100])
        self.assertEqual(run([m4, out, "-"], input="AUROCKS_PARSE({{{1+x}}})\n").strip(), "null")
        result = run([m4, out, "-"], input="define({{{AUROCKS_VISIT_NODE}}}, {{{[$4]}}})dnl\nAUROCKS_VISIT({{{1+2}}})\n")
        self.assertTrue(result.startswith("[Integer][Wrap][Integer][Add]"), result[:100])
        result = run([m4, out, "-"], input="AUROCKS_PARSE_FILE({{{" + str(self.work / "input.txt") + "}}})\n")
        self.assertEqual(json.loads(result)["root"]["kind"], "Add")
        grammar = self.work / "quotes.g"
        grammar.write_text("%start S\n%%\nS: /.+/;\n%%\n")
        self.generate("m4", out, grammar, modes="")
        self.assertEqual(json.loads(run([m4, out, "-"], input="AUROCKS_PARSE({{{a'b}}})\n"))["root"]["text"], "a'b")
        grammar.write_text("%actions ON\n%start S\n%%\nS: /.+/ { [action:$TOKEN] };\n%%\n")
        self.generate("m4", out, grammar, modes="")
        result = run([m4, out, "-"], input="AUROCKS_PARSE({{{abc}}})\n")
        self.assertTrue(result.lstrip().startswith("[action:616263]"), result[:100])


if __name__ == "__main__":
    unittest.main(verbosity=2)
