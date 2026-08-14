#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "lexer/Lexer.hpp"

using jc::Lexer;
using jc::Token;
using jc::TokenType;

namespace {

std::vector<TokenType> typesOf(const std::vector<Token>& tokens) {
    std::vector<TokenType> types;
    types.reserve(tokens.size());
    for (const auto& t : tokens) types.push_back(t.type);
    return types;
}

}  // namespace

TEST_CASE("empty source yields only EOF") {
    Lexer lexer("");
    auto tokens = lexer.tokenize();
    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].type == TokenType::EndOfFile);
    CHECK(lexer.errors().empty());
}

TEST_CASE("keywords are recognized, not left as identifiers") {
    Lexer lexer("class extends public private static void int boolean double "
                "if else while for return new this true false");
    auto tokens = lexer.tokenize();
    std::vector<TokenType> expected = {
        TokenType::KwClass,   TokenType::KwExtends, TokenType::KwPublic,
        TokenType::KwPrivate, TokenType::KwStatic,  TokenType::KwVoid,
        TokenType::KwInt,     TokenType::KwBoolean, TokenType::KwDouble,
        TokenType::KwIf,      TokenType::KwElse,    TokenType::KwWhile,
        TokenType::KwFor,     TokenType::KwReturn,  TokenType::KwNew,
        TokenType::KwThis,    TokenType::KwTrue,    TokenType::KwFalse,
        TokenType::EndOfFile,
    };
    CHECK(typesOf(tokens) == expected);
}

TEST_CASE("identifiers vs keyword-prefixed identifiers") {
    Lexer lexer("intValue classy _foo foo2");
    auto tokens = lexer.tokenize();
    REQUIRE(tokens.size() == 5);
    CHECK(tokens[0].type == TokenType::Identifier);
    CHECK(tokens[0].text == "intValue");
    CHECK(tokens[1].type == TokenType::Identifier);
    CHECK(tokens[1].text == "classy");
    CHECK(tokens[2].type == TokenType::Identifier);
    CHECK(tokens[2].text == "_foo");
    CHECK(tokens[3].type == TokenType::Identifier);
    CHECK(tokens[3].text == "foo2");
}

TEST_CASE("integer and double literals") {
    Lexer lexer("42 3.14 0 100.001");
    auto tokens = lexer.tokenize();
    REQUIRE(tokens.size() == 5);
    CHECK(tokens[0].type == TokenType::IntLiteral);
    CHECK(tokens[0].text == "42");
    CHECK(tokens[1].type == TokenType::DoubleLiteral);
    CHECK(tokens[1].text == "3.14");
    CHECK(tokens[2].type == TokenType::IntLiteral);
    CHECK(tokens[2].text == "0");
    CHECK(tokens[3].type == TokenType::DoubleLiteral);
    CHECK(tokens[3].text == "100.001");
}

TEST_CASE("string literal with escapes decodes correctly") {
    Lexer lexer(R"("hello\nworld \"quoted\"")");
    auto tokens = lexer.tokenize();
    REQUIRE(tokens.size() == 2);
    CHECK(tokens[0].type == TokenType::StringLiteral);
    CHECK(tokens[0].stringValue == "hello\nworld \"quoted\"");
}

TEST_CASE("operators, including two-character forms") {
    Lexer lexer("+ - * / % = == != < <= > >= && || !");
    auto tokens = lexer.tokenize();
    std::vector<TokenType> expected = {
        TokenType::Plus,  TokenType::Minus, TokenType::Star,   TokenType::Slash,
        TokenType::Percent, TokenType::Assign, TokenType::EqEq, TokenType::NotEq,
        TokenType::Lt,    TokenType::LtEq,  TokenType::Gt,     TokenType::GtEq,
        TokenType::AndAnd, TokenType::OrOr, TokenType::Not,    TokenType::EndOfFile,
    };
    CHECK(typesOf(tokens) == expected);
}

TEST_CASE("punctuation") {
    Lexer lexer("{}()[];,.");
    auto tokens = lexer.tokenize();
    std::vector<TokenType> expected = {
        TokenType::LBrace, TokenType::RBrace,   TokenType::LParen,    TokenType::RParen,
        TokenType::LBracket, TokenType::RBracket, TokenType::Semicolon, TokenType::Comma,
        TokenType::Dot,    TokenType::EndOfFile,
    };
    CHECK(typesOf(tokens) == expected);
}

TEST_CASE("line comments and block comments are skipped") {
    Lexer lexer("int x; // trailing comment\n/* block\n comment */ int y;");
    auto tokens = lexer.tokenize();
    std::vector<TokenType> expected = {
        TokenType::KwInt, TokenType::Identifier, TokenType::Semicolon,
        TokenType::KwInt, TokenType::Identifier, TokenType::Semicolon,
        TokenType::EndOfFile,
    };
    CHECK(typesOf(tokens) == expected);
    CHECK(lexer.errors().empty());
}

TEST_CASE("a small real snippet tokenizes end to end") {
    Lexer lexer(
        "public class Foo {\n"
        "    int x;\n"
        "    public int bar(int y) {\n"
        "        return x + y;\n"
        "    }\n"
        "}\n");
    auto tokens = lexer.tokenize();
    CHECK(lexer.errors().empty());
    CHECK(tokens.back().type == TokenType::EndOfFile);
    // Spot-check a few tokens rather than the whole sequence.
    CHECK(tokens[0].type == TokenType::KwPublic);
    CHECK(tokens[1].type == TokenType::KwClass);
    CHECK(tokens[2].type == TokenType::Identifier);
    CHECK(tokens[2].text == "Foo");
}

TEST_CASE("line and column tracking across newlines") {
    Lexer lexer("int x;\nint y;");
    auto tokens = lexer.tokenize();
    // "int" on line 1 starts at column 1.
    CHECK(tokens[0].line == 1);
    CHECK(tokens[0].col == 1);
    // second "int" is on line 2, column 1.
    auto it = std::find_if(tokens.begin(), tokens.end(), [](const Token& t) {
        return t.type == TokenType::KwInt && t.line == 2;
    });
    REQUIRE(it != tokens.end());
    CHECK(it->col == 1);
}

TEST_CASE("unknown character produces an Error token and a diagnostic") {
    Lexer lexer("int x = 1 @ 2;");
    auto tokens = lexer.tokenize();
    REQUIRE(lexer.errors().size() == 1);
    CHECK(lexer.errors()[0].message.find('@') != std::string::npos);

    bool sawError = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::Error) sawError = true;
    }
    CHECK(sawError);
}

TEST_CASE("unterminated string literal produces a diagnostic") {
    Lexer lexer("\"never closed");
    auto tokens = lexer.tokenize();
    REQUIRE(tokens.size() == 2);
    CHECK(tokens[0].type == TokenType::Error);
    REQUIRE(lexer.errors().size() == 1);
    CHECK(lexer.errors()[0].message == "unterminated string literal");
}

TEST_CASE("unterminated block comment produces a diagnostic but does not hang") {
    Lexer lexer("int x; /* never closed");
    auto tokens = lexer.tokenize();
    CHECK(tokens.back().type == TokenType::EndOfFile);
    REQUIRE(lexer.errors().size() == 1);
    CHECK(lexer.errors()[0].message == "unterminated block comment");
}
