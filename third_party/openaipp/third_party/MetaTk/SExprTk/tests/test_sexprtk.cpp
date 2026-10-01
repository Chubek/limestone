#define CATCH_CONFIG_MAIN
#include <catch_amalgamated.hpp>

#include "SExprTk.hpp"

using namespace sexprtk;

TEST_CASE("parse atoms and round-trip", "[parse]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(alpha 12 \"beta\" #t)"));
    REQUIRE(cartable.root.cells.size() == 1);
    REQUIRE(cartable.ok());
    CHECK(Serializer::to_string(cartable.root) == "((alpha 12 \"beta\" #t))");
}

TEST_CASE("parse nested lists and numbers", "[parse]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(a (b 1 2.5) (c (d)))"));
    REQUIRE(cartable.root.cells.size() == 1);
    CHECK(cartable.root.cells[0].head.as_list().size() == 3);
    CHECK(Serializer::to_string(cartable.root) == "((a (b 1 2.5) (c (d))))");
}

TEST_CASE("comments and whitespace are ignored", "[parse]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(a ; comment\n b)"));
    REQUIRE(cartable.ok());
    CHECK(Serializer::to_string(cartable.root) == "((a b))");
}

TEST_CASE("quotes are expanded", "[parse]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(a 'b)"));
    REQUIRE(cartable.ok());
    CHECK(Serializer::to_string(cartable.root) == "((a (quote b)))");
}

TEST_CASE("atoms expose correct kinds", "[atom]") {
    CHECK(Atom(true).is_bool());
    CHECK(Atom(std::int64_t{42}).is_int());
    CHECK(Atom(3.14).is_float());
    CHECK(Atom(std::string("x")).is_string());
    CHECK(Atom(std::string("y"), NodeKind::Symbol).is_symbol());
    CHECK(Atom(std::make_shared<List>()).is_list());
    CHECK(Atom{}.is_nil());
}

TEST_CASE("shape analyzer counts via Analyzer interface", "[analysis]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(a (b 1 2) (c))"));

    ShapeAnalyzer shape;
    // use through the abstract base class
    const Analyzer& base = shape;
    auto result = base.analyze(cartable);
    CHECK(result.get_count("atoms") == 5);
    CHECK(result.get_count("lists") == 3);
    CHECK(result.get_count("depth") == 5);
    CHECK(shape.name() == "shape");
}

TEST_CASE("symbol analyzer collects symbols", "[analysis]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(a (b 1 2) (c b))"));

    SymbolAnalyzer syms;
    auto result = syms.analyze(cartable);
    CHECK(result.get_count("unique-symbols") == 3);
    CHECK(SymbolAnalyzer::has_symbol(cartable.root, "b"));
    CHECK_FALSE(SymbolAnalyzer::has_symbol(cartable.root, "z"));
}

TEST_CASE("serializer escapes strings", "[serialize]") {
    Atom a{std::string("line\nbreak")};
    CHECK(Serializer::to_string(a) == "\"line\\nbreak\"");
}

TEST_CASE("json output", "[json]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(x 1 \"y\")"));
    auto json = cartable.to_json();
    CHECK(json.find("\"x\"") != std::string::npos);
    CHECK(json.find("1") != std::string::npos);
    CHECK(json.find("\"y\"") != std::string::npos);
}

TEST_CASE("emit xas events", "[xas]") {
    XASEventDispatcher disp;
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(x y)"), &disp);
    REQUIRE(cartable.events.size() >= 5); // doc-begin, list-begin, 2 atoms, list-end, doc-end
    CHECK(disp.buffered.size() == cartable.events.size());
    CHECK(disp.front().kind == SEXPRTK_XAS_EVENT_DOCUMENT_BEGIN);
    CHECK(disp.buffered[1].kind == SEXPRTK_XAS_EVENT_LIST_BEGIN);
    CHECK(disp.back().kind == SEXPRTK_XAS_EVENT_DOCUMENT_END);
}

TEST_CASE("xas protocol: kind names round-trip", "[xas][protocol]") {
    CHECK(std::string(sexprtk_xas_event_kind_name(SEXPRTK_XAS_EVENT_ATOM)) == "atom");
    CHECK(std::string(sexprtk_xas_event_kind_name(SEXPRTK_XAS_EVENT_LIST_BEGIN)) == "list-begin");
    CHECK(sexprtk_xas_event_kind_from_name("atom") == SEXPRTK_XAS_EVENT_ATOM);
    CHECK(sexprtk_xas_event_kind_from_name("list-end") == SEXPRTK_XAS_EVENT_LIST_END);
    CHECK(sexprtk_xas_event_kind_from_name("bogus") == -1);
    CHECK(sexprtk_xas_event_kind_valid(SEXPRTK_XAS_EVENT_QUOTE) != 0);
    CHECK(sexprtk_xas_event_kind_valid(255) == 0);
}

TEST_CASE("xas protocol: datagram frame round-trip via C interface", "[xas][protocol]") {
    sexprtk_xas_event ev;
    sexprtk_xas_event_init(&ev);
    ev.sequence = 42;
    ev.kind = SEXPRTK_XAS_EVENT_LIST_BEGIN;
    ev.line = 3;
    ev.column = 7;
    ev.source_id = 9;
    const char* payload = "(";
    ev.payload = payload;
    ev.payload_length = 1;

    unsigned char storage[256];
    sexprtk_xas_frame frame;
    sexprtk_xas_frame_init(&frame);
    frame.bytes = storage;
    frame.capacity = sizeof(storage);

    REQUIRE(sexprtk_xas_frame_encode(&ev, &frame) == SEXPRTK_XAS_OK);
    CHECK(frame.length == SEXPRTK_XAS_HEADER_SIZE + 1);
    REQUIRE(sexprtk_xas_frame_validate(&frame) == SEXPRTK_XAS_OK);
    CHECK(sexprtk_xas_frame_payload_length(&frame) == 1);

    sexprtk_xas_event decoded;
    REQUIRE(sexprtk_xas_frame_decode(&frame, &decoded) == SEXPRTK_XAS_OK);
    CHECK(decoded.sequence == 42);
    CHECK(decoded.kind == SEXPRTK_XAS_EVENT_LIST_BEGIN);
    CHECK(decoded.line == 3);
    CHECK(decoded.column == 7);
    CHECK(decoded.source_id == 9);
    REQUIRE(decoded.payload_length == 1);
    CHECK(decoded.payload[0] == '(');
}

TEST_CASE("xas protocol: frame validation rejects garbage", "[xas][protocol]") {
    unsigned char garbage[4] = {0, 1, 2, 3};
    sexprtk_xas_frame frame;
    sexprtk_xas_frame_init(&frame);
    frame.bytes = garbage;
    frame.length = sizeof(garbage);
    CHECK(sexprtk_xas_frame_validate(&frame) == SEXPRTK_XAS_ERR_TRUNCATED);

    unsigned char bad_magic[SEXPRTK_XAS_HEADER_SIZE] = {};
    frame.bytes = bad_magic;
    frame.length = sizeof(bad_magic);
    CHECK(sexprtk_xas_frame_validate(&frame) == SEXPRTK_XAS_ERR_BAD_MAGIC);
}

TEST_CASE("xas protocol: pump moves events from source to sink", "[xas][protocol]") {
    struct Ctx {
        int produced = 0;
        std::vector<sexprtk_xas_event_kind> received;
    } ctx;

    sexprtk_xas_event_source source;
    source.userdata = &ctx;
    source.next = [](sexprtk_xas_event* ev, void* ud) -> int {
        auto* c = static_cast<Ctx*>(ud);
        if (c->produced >= 3) return SEXPRTK_XAS_ERR_TRUNCATED;
        sexprtk_xas_event_init(ev);
        ev->kind = static_cast<std::uint8_t>(
            c->produced == 0 ? SEXPRTK_XAS_EVENT_LIST_BEGIN :
            c->produced == 1 ? SEXPRTK_XAS_EVENT_ATOM : SEXPRTK_XAS_EVENT_LIST_END);
        ++c->produced;
        return SEXPRTK_XAS_OK;
    };

    sexprtk_xas_event_sink sink;
    sink.userdata = &ctx;
    sink.handle = [](const sexprtk_xas_event* ev, void* ud) -> int {
        static_cast<Ctx*>(ud)->received.push_back(
            static_cast<sexprtk_xas_event_kind>(ev->kind));
        return 0;
    };

    CHECK(sexprtk_xas_pump(&source, &sink) == SEXPRTK_XAS_OK);
    REQUIRE(ctx.received.size() == 3);
    CHECK(ctx.received[0] == SEXPRTK_XAS_EVENT_LIST_BEGIN);
    CHECK(ctx.received[1] == SEXPRTK_XAS_EVENT_ATOM);
    CHECK(ctx.received[2] == SEXPRTK_XAS_EVENT_LIST_END);
}

TEST_CASE("xas protocol: dispatcher adapts to C sink", "[xas][protocol]") {
    XASEventDispatcher disp;
    auto c_sink = disp.as_c_sink();

    sexprtk_xas_event ev;
    sexprtk_xas_event_init(&ev);
    ev.kind = SEXPRTK_XAS_EVENT_ATOM;
    ev.sequence = 99;
    const char* payload = "hello";
    ev.payload = payload;
    ev.payload_length = 5;

    CHECK(c_sink.handle(&ev, c_sink.userdata) == SEXPRTK_XAS_OK);
    REQUIRE(disp.size() == 1);
    CHECK(disp.back().kind == SEXPRTK_XAS_EVENT_ATOM);
    CHECK(disp.back().sequence == 99);
    CHECK(disp.back().payload == "hello");
}

TEST_CASE("package manifest serializes", "[package]") {
    PackageManifest manifest;
    manifest.name = "demo";
    manifest.fields["mode"] = "test";
    auto toml = manifest.to_toml();
    CHECK(toml.find("name = \"demo\"") != std::string::npos);
    CHECK(toml.find("mode = \"test\"") != std::string::npos);

    auto parsed = PackageManifest::from_toml(toml);
    CHECK(parsed.name == "demo");
    CHECK(parsed.fields["mode"] == "test");
}

TEST_CASE("flatten transformer via Transformer interface", "[transform]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(a (b c) d)"));

    FlattenTransformer flatten;
    const Transformer& base = flatten;
    auto out = base.transform(cartable);
    CHECK(out.root.size() == 4);
    CHECK(flatten.name() == "flatten");
}

TEST_CASE("constant-fold transformer folds integer arithmetic", "[transform]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(+ 1 2 3)"));

    ConstantFoldTransformer fold;
    auto out = fold.transform(cartable);
    CHECK(Serializer::to_string(out.root) == "(6)");
}

TEST_CASE("constant-fold transformer leaves mixed forms alone", "[transform]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(+ 1 x 3)"));

    ConstantFoldTransformer fold;
    auto out = fold.transform(cartable);
    CHECK(Serializer::to_string(out.root) == "((+ 1 x 3))");
}

TEST_CASE("iterator traversal", "[iterator]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(a b c)"));
    Iterator it(cartable.root);
    std::size_t n = 0;
    while (!it.done()) { ++it; ++n; }
    CHECK(n == 1);
}

TEST_CASE("kernel dispatch through abstract Kernel", "[kernel]") {
    SExprTk rt;
    auto source = Source::from_string("(+ 1 2)");

    LuaKernel lua;
    S7Kernel s7;
    const Kernel& k1 = lua;
    const Kernel& k2 = s7;
    CHECK(k1.name() == "lua");
    CHECK(k2.name() == "s7");

    // Evaluate through the base interface; both backends should give
    // the program meaning (3) when compiled with runtime support.
    auto sem1 = k1.evaluate(rt.parse(source));
    auto sem2 = k2.evaluate(rt.parse(source));
#ifdef SEXPRTK_WITH_LUA
    CHECK(sem1.ok());
    CHECK(sem1.rendered == "3");
#endif
#ifdef SEXPRTK_WITH_S7
    CHECK(sem2.ok());
    CHECK(sem2.rendered == "3");
#endif
}

TEST_CASE("semanticizers give meaning to programs", "[semanticizer]") {
    SExprTk rt;
    auto source = Source::from_string("(* 6 7)");

    LuaKernelSemanticizer lua_sem;
    S7KernelSemanticizer s7_sem;
    const Semanticizer& s1 = lua_sem;
    const Semanticizer& s2 = s7_sem;
    CHECK(s1.name() == "lua-semanticizer");
    CHECK(s2.name() == "s7-semanticizer");

    auto m1 = rt.semanticize(source, s1);
    auto m2 = rt.semanticize(source, s2);
#ifdef SEXPRTK_WITH_LUA
    REQUIRE(m1.ok());
    CHECK(m1.rendered == "42");
    CHECK(m1.value.is_int());
    CHECK(m1.value.as_int() == 42);
#endif
#ifdef SEXPRTK_WITH_S7
    REQUIRE(m2.ok());
    CHECK(m2.rendered == "42");
    CHECK(m2.value.is_int());
    CHECK(m2.value.as_int() == 42);
#endif
}

TEST_CASE("lazy stream", "[stream]") {
    LazyStream ls("hello");
    CHECK(*ls.peek() == 'h');
    CHECK(*ls.take() == 'h');
    CHECK(*ls.take() == 'e');
    ls.append("world");
    CHECK(ls.buffer == "helloworld");
}

TEST_CASE("unterminated list produces error", "[error]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(a (b"));
    CHECK_FALSE(cartable.ok());
    CHECK(!cartable.errors.empty());
}

TEST_CASE("package manifest from_toml round-trip", "[package]") {
    std::string toml = "name = \"pkg\"\nversion = \"1.0.0\"\nentry = \"main.sx\"\n";
    auto m = PackageManifest::from_toml(toml);
    CHECK(m.name == "pkg");
    CHECK(m.version == "1.0.0");
    CHECK(m.entry == "main.sx");
}
