#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

#include "ast/Decl.hpp"
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "sema/SemanticAnalyzer.hpp"

using jc::CompilationUnit;
using jc::Lexer;
using jc::Parser;
using jc::SemaError;
using jc::SemanticAnalyzer;

namespace {

struct AnalyzeResult {
    std::unique_ptr<CompilationUnit> unit;
    std::vector<SemaError> errors;
    bool ok = false;
};

AnalyzeResult analyze(const std::string& src) {
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    auto unit = parser.parseCompilationUnit();
    if (!unit || !parser.errors().empty()) {
        return AnalyzeResult{std::move(unit), {}, false};
    }
    SemanticAnalyzer sema;
    bool ok = sema.analyze(*unit);
    return AnalyzeResult{std::move(unit), sema.errors(), ok};
}

}  // namespace

TEST_CASE("valid program with fields, constructor, method, println") {
    auto r = analyze(
        "public class Counter {"
        "  int value;"
        "  Counter(int start) { this.value = start; }"
        "  public int get() { return this.value; }"
        "  public void bump() { this.value = this.value + 1; }"
        "  public static void main(String[] args) {"
        "    Counter c = new Counter(0);"
        "    c.bump();"
        "    System.out.println(c.get());"
        "  }"
        "}");
    REQUIRE(r.unit != nullptr);
    CHECK(r.ok);
    CHECK(r.errors.empty());
}

TEST_CASE("println accepts int, double, boolean, and string") {
    auto r = analyze(
        "public class Foo { void m() {"
        "  System.out.println(1);"
        "  System.out.println(1.5);"
        "  System.out.println(true);"
        "  System.out.println(\"hi\");"
        "} }");
    CHECK(r.ok);
    CHECK(r.errors.empty());
}

TEST_CASE("unknown identifier is a diagnostic") {
    auto r = analyze("public class Foo { void m() { int x = y; } }");
    CHECK_FALSE(r.ok);
    REQUIRE(r.errors.size() >= 1);
    CHECK(r.errors[0].message.find("unknown identifier") != std::string::npos);
}

TEST_CASE("local var initializer type mismatch") {
    auto r = analyze("public class Foo { void m() { boolean b = 1; } }");
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.errors.empty());
}

TEST_CASE("int widens to double but not the reverse") {
    auto ok = analyze("public class Foo { void m() { double d = 1; } }");
    CHECK(ok.ok);

    auto bad = analyze("public class Foo { void m() { int i = 1.5; } }");
    CHECK_FALSE(bad.ok);
}

TEST_CASE("assignment type mismatch is a diagnostic") {
    auto r = analyze("public class Foo { void m() { int x = 1; x = true; } }");
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.errors.empty());
}

TEST_CASE("method call with wrong argument count") {
    auto r = analyze(
        "public class Foo { int add(int a, int b) { return a + b; } "
        "void m() { add(1); } }");
    CHECK_FALSE(r.ok);
    bool found = false;
    for (auto& e : r.errors) {
        if (e.message.find("expects") != std::string::npos) found = true;
    }
    CHECK(found);
}

TEST_CASE("method call with wrong argument type") {
    auto r = analyze(
        "public class Foo { int add(int a, int b) { return a + b; } "
        "void m() { add(1, true); } }");
    CHECK_FALSE(r.ok);
}

TEST_CASE("unknown method is a diagnostic") {
    auto r = analyze("public class Foo { void m() { nope(); } }");
    CHECK_FALSE(r.ok);
}

TEST_CASE("'this' in a static context is a diagnostic") {
    auto r = analyze("public class Foo { int x; static void m() { int y = this.x; } }");
    CHECK_FALSE(r.ok);
}

TEST_CASE("instance field access from a static context is a diagnostic") {
    auto r = analyze("public class Foo { int x; static void m() { int y = x; } }");
    CHECK_FALSE(r.ok);
}

TEST_CASE("if/while conditions must be boolean") {
    auto r = analyze("public class Foo { void m() { if (1) { } } }");
    CHECK_FALSE(r.ok);

    auto r2 = analyze("public class Foo { void m() { while (1) { } } }");
    CHECK_FALSE(r2.ok);
}

TEST_CASE("return type checking") {
    auto missing = analyze("public class Foo { int m() { return; } }");
    CHECK_FALSE(missing.ok);

    auto unexpected = analyze("public class Foo { void m() { return 1; } }");
    CHECK_FALSE(unexpected.ok);

    auto mismatch = analyze("public class Foo { int m() { return true; } }");
    CHECK_FALSE(mismatch.ok);

    auto ok = analyze("public class Foo { int m() { return 1; } }");
    CHECK(ok.ok);
}

TEST_CASE("duplicate field is a diagnostic") {
    auto r = analyze("public class Foo { int x; int x; }");
    CHECK_FALSE(r.ok);
}

TEST_CASE("duplicate method (no overloading) is a diagnostic") {
    auto r = analyze("public class Foo { void m() { } int m(int a) { return a; } }");
    CHECK_FALSE(r.ok);
}

TEST_CASE("redeclaring a local in the same scope is a diagnostic, shadowing in a nested scope is not") {
    auto dup = analyze("public class Foo { void m() { int x = 1; int x = 2; } }");
    CHECK_FALSE(dup.ok);

    auto shadow = analyze("public class Foo { void m() { int x = 1; if (true) { int x = 2; } } }");
    CHECK(shadow.ok);
}

TEST_CASE("arrays are rejected except main's String[] args") {
    auto mainOk = analyze("public class Foo { public static void main(String[] args) { } }");
    CHECK(mainOk.ok);

    auto fieldBad = analyze("public class Foo { String[] names; }");
    CHECK_FALSE(fieldBad.ok);

    auto localBad = analyze("public class Foo { void m() { String[] names; } }");
    CHECK_FALSE(localBad.ok);
}

TEST_CASE("local variable slots account for double taking two slots") {
    auto r = analyze("public class Foo { void m(double a, int b) { double c; int d; } }");
    REQUIRE(r.ok);
    const jc::MethodDecl& m = r.unit->classDecl.methods[0];
    // this(1) + a(2 slots) + b(1) + c(2 slots) + d(1) = slot count 7
    CHECK(m.maxLocals == 7);
}

TEST_CASE("string concatenation with + is explicitly rejected for now") {
    auto r = analyze("public class Foo { void m() { System.out.println(\"a\" + \"b\"); } }");
    CHECK_FALSE(r.ok);
}
