#pragma once

#include <string>

namespace jc {

enum class TokenType {
    // literals
    IntLiteral,
    DoubleLiteral,
    StringLiteral,
    Identifier,

    // keywords
    KwClass,
    KwExtends,
    KwPublic,
    KwPrivate,
    KwStatic,
    KwVoid,
    KwInt,
    KwBoolean,
    KwDouble,
    KwIf,
    KwElse,
    KwWhile,
    KwFor,
    KwReturn,
    KwNew,
    KwThis,
    KwTrue,
    KwFalse,

    // punctuation
    LBrace,
    RBrace,
    LParen,
    RParen,
    LBracket,
    RBracket,
    Semicolon,
    Comma,
    Dot,

    // operators
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Assign,
    EqEq,
    NotEq,
    Lt,
    LtEq,
    Gt,
    GtEq,
    AndAnd,
    OrOr,
    Not,

    EndOfFile,
    Error,
};

const char* tokenTypeName(TokenType type);

struct Token {
    TokenType type = TokenType::Error;
    // Raw lexeme text: verbatim for identifiers/keywords/operators/numeric
    // literals. For a StringLiteral, this is the source text including the
    // surrounding quotes; decoded contents are in stringValue.
    std::string text;
    std::string stringValue;
    int line = 0;
    int col = 0;
};

}  // namespace jc
