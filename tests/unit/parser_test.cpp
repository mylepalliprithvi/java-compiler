#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ast/AstPrinter.hpp"
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"

using jc::CompilationUnit;
using jc::Lexer;
using jc::ParseError;
using jc::Parser;
using jc::printAst;

namespace {

struct ParseResult {
    std::unique_ptr<CompilationUnit> unit;
    std::vector<ParseError> errors;
};

ParseResult parse(const std::string& src) {
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    auto unit = parser.parseCompilationUnit();
    return ParseResult{std::move(unit), parser.errors()};
}

}  // namespace

TEST_CASE("empty class") {
    auto r = parse("public class Foo { }");
    REQUIRE(r.unit != nullptr);
    CHECK(r.errors.empty());
    CHECK(printAst(*r.unit) == "(class Foo (fields) (ctors) (methods))");
}

TEST_CASE("class with extends") {
    auto r = parse("public class Foo extends Bar { }");
    REQUIRE(r.unit != nullptr);
    CHECK(r.errors.empty());
    CHECK(printAst(*r.unit) == "(class Foo extends Bar (fields) (ctors) (methods))");
}

TEST_CASE("field declaration") {
    auto r = parse("public class Foo { int x; double y; }");
    REQUIRE(r.unit != nullptr);
    CHECK(r.errors.empty());
    CHECK(printAst(*r.unit) ==
          "(class Foo (fields (field int x) (field double y)) (ctors) (methods))");
}

TEST_CASE("constructor") {
    auto r = parse("public class Foo { Foo(int x) { this.x = x; } }");
    REQUIRE(r.unit != nullptr);
    CHECK(r.errors.empty());
    CHECK(printAst(*r.unit) ==
          "(class Foo (fields) (ctors (ctor (params (param int x)) "
          "(block (expr-stmt (assign (field-access this x) x)))))"
          " (methods))");
}

TEST_CASE("method with return and binary expression") {
    auto r = parse("public class Foo { public int add(int a, int b) { return a + b; } }");
    REQUIRE(r.unit != nullptr);
    CHECK(r.errors.empty());
    CHECK(printAst(*r.unit) ==
          "(class Foo (fields) (ctors) (methods (method int add "
          "(params (param int a) (param int b)) (block (return (binary + a b))))))");
}

TEST_CASE("static method (for a main entry point)") {
    auto r = parse("public class Foo { public static void main(String[] args) { } }");
    REQUIRE(r.unit != nullptr);
    CHECK(r.errors.empty());
    CHECK(printAst(*r.unit) ==
          "(class Foo (fields) (ctors) (methods (method static void main "
          "(params (param String[] args)) (block))))");
}

TEST_CASE("expression operator precedence") {
    auto r = parse("public class Foo { void m() { int x = 1 + 2 * 3; } }");
    REQUIRE(r.unit != nullptr);
    CHECK(r.errors.empty());
    CHECK(printAst(*r.unit) ==
          "(class Foo (fields) (ctors) (methods (method void m (params) "
          "(block (local int x (binary + 1 (binary * 2 3)))))))");
}

TEST_CASE("if/else and while and for") {
    auto r = parse(
        "public class Foo { void m() { "
        "if (x < 1) { x = 1; } else { x = 2; } "
        "while (x < 10) { x = x + 1; } "
        "for (int i = 0; i < 10; i = i + 1) { x = x + i; } "
        "} }");
    REQUIRE(r.unit != nullptr);
    CHECK(r.errors.empty());
    // Spot-check via presence of key sub-expressions rather than the whole tree.
    std::string printed = printAst(*r.unit);
    CHECK(printed.find("(if (binary < x 1)") != std::string::npos);
    CHECK(printed.find("(while (binary < x 10)") != std::string::npos);
    CHECK(printed.find("(for (local int i 0)") != std::string::npos);
}

TEST_CASE("method call chain: System.out.println(x)") {
    auto r = parse("public class Foo { void m() { System.out.println(x); } }");
    REQUIRE(r.unit != nullptr);
    CHECK(r.errors.empty());
    CHECK(printAst(*r.unit) ==
          "(class Foo (fields) (ctors) (methods (method void m (params) "
          "(block (expr-stmt (call (field-access System out) println x))))))");
}

TEST_CASE("new expression and unqualified method call") {
    auto r = parse("public class Foo { void m() { Foo f = new Foo(1, 2); f.m(); } }");
    REQUIRE(r.unit != nullptr);
    CHECK(r.errors.empty());
    CHECK(printAst(*r.unit) ==
          "(class Foo (fields) (ctors) (methods (method void m (params) "
          "(block (local Foo f (new Foo 1 2)) (expr-stmt (call f m))))))");
}

TEST_CASE("unary minus and logical not") {
    auto r = parse("public class Foo { void m() { int x = -1; boolean b = !true; } }");
    REQUIRE(r.unit != nullptr);
    CHECK(r.errors.empty());
    CHECK(printAst(*r.unit) ==
          "(class Foo (fields) (ctors) (methods (method void m (params) "
          "(block (local int x (unary - 1)) (local boolean b (unary ! true))))))");
}

TEST_CASE("invalid assignment target is a diagnostic, not a crash") {
    auto r = parse("public class Foo { void m() { 1 = 2; } }");
    REQUIRE(r.unit != nullptr);
    CHECK_FALSE(r.errors.empty());
}

TEST_CASE("parser recovers after a malformed member and keeps parsing later ones") {
    auto r = parse("public class Foo { int ; int y; }");
    REQUIRE(r.unit != nullptr);
    CHECK_FALSE(r.errors.empty());
    // Recovery should still pick up the second, well-formed field.
    CHECK(printAst(*r.unit).find("(field int y)") != std::string::npos);
}

TEST_CASE("missing closing brace on class is a diagnostic") {
    auto r = parse("public class Foo {");
    CHECK_FALSE(r.errors.empty());
}
