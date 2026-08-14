
#include "lexer/Lexer.hpp"

#include <cctype>
#include <unordered_map>

namespace jc {

const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::IntLiteral: return "IntLiteral";
        case TokenType::DoubleLiteral: return "DoubleLiteral";
        case TokenType::StringLiteral: return "StringLiteral";
        case TokenType::Identifier: return "Identifier";
        case TokenType::KwClass: return "KwClass";
        case TokenType::KwExtends: return "KwExtends";
        case TokenType::KwPublic: return "KwPublic";
        case TokenType::KwPrivate: return "KwPrivate";
        case TokenType::KwStatic: return "KwStatic";
        case TokenType::KwVoid: return "KwVoid";
        case TokenType::KwInt: return "KwInt";
        case TokenType::KwBoolean: return "KwBoolean";
        case TokenType::KwDouble: return "KwDouble";
        case TokenType::KwIf: return "KwIf";
        case TokenType::KwElse: return "KwElse";
        case TokenType::KwWhile: return "KwWhile";
        case TokenType::KwFor: return "KwFor";
        case TokenType::KwReturn: return "KwReturn";
        case TokenType::KwNew: return "KwNew";
        case TokenType::KwThis: return "KwThis";
        case TokenType::KwTrue: return "KwTrue";
        case TokenType::KwFalse: return "KwFalse";
        case TokenType::LBrace: return "LBrace";
        case TokenType::RBrace: return "RBrace";
        case TokenType::LParen: return "LParen";
        case TokenType::RParen: return "RParen";
        case TokenType::LBracket: return "LBracket";
        case TokenType::RBracket: return "RBracket";
        case TokenType::Semicolon: return "Semicolon";
        case TokenType::Comma: return "Comma";
        case TokenType::Dot: return "Dot";
        case TokenType::Plus: return "Plus";
        case TokenType::Minus: return "Minus";
        case TokenType::Star: return "Star";
        case TokenType::Slash: return "Slash";
        case TokenType::Percent: return "Percent";
        case TokenType::Assign: return "Assign";
        case TokenType::EqEq: return "EqEq";
        case TokenType::NotEq: return "NotEq";
        case TokenType::Lt: return "Lt";
        case TokenType::LtEq: return "LtEq";
        case TokenType::Gt: return "Gt";
        case TokenType::GtEq: return "GtEq";
        case TokenType::AndAnd: return "AndAnd";
        case TokenType::OrOr: return "OrOr";
        case TokenType::Not: return "Not";
        case TokenType::EndOfFile: return "EndOfFile";
        case TokenType::Error: return "Error";
    }
    return "Unknown";
}

namespace {

const std::unordered_map<std::string, TokenType>& keywords() {
    static const std::unordered_map<std::string, TokenType> kKeywords = {
        {"class", TokenType::KwClass},     {"extends", TokenType::KwExtends},
        {"public", TokenType::KwPublic},   {"private", TokenType::KwPrivate},
        {"static", TokenType::KwStatic},   {"void", TokenType::KwVoid},
        {"int", TokenType::KwInt},         {"boolean", TokenType::KwBoolean},
        {"double", TokenType::KwDouble},   {"if", TokenType::KwIf},
        {"else", TokenType::KwElse},       {"while", TokenType::KwWhile},
        {"for", TokenType::KwFor},         {"return", TokenType::KwReturn},
        {"new", TokenType::KwNew},         {"this", TokenType::KwThis},
        {"true", TokenType::KwTrue},       {"false", TokenType::KwFalse},
    };
    return kKeywords;
}

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentCont(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

}  // namespace

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

char Lexer::peek(int offset) const {
    size_t p = pos_ + static_cast<size_t>(offset);
    if (p >= source_.size()) return '\0';
    return source_[p];
}

bool Lexer::isAtEnd() const { return pos_ >= source_.size(); }

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') {
        line_++;
        col_ = 1;
    } else {
        col_++;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd() || peek() != expected) return false;
    advance();
    return true;
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            while (!isAtEnd() && peek() != '\n') advance();
        } else if (c == '/' && peek(1) == '*') {
            int startLine = line_;
            int startCol = col_;
            advance();
            advance();
            bool closed = false;
            while (!isAtEnd()) {
                if (peek() == '*' && peek(1) == '/') {
                    advance();
                    advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) {
                errors_.push_back({startLine, startCol, "unterminated block comment"});
            }
        } else {
            break;
        }
    }
}

Token Lexer::errorToken(const std::string& message, int line, int col, std::string text) {
    errors_.push_back({line, col, message});
    Token t;
    t.type = TokenType::Error;
    t.text = std::move(text);
    t.stringValue = message;
    t.line = line;
    t.col = col;
    return t;
}

Token Lexer::lexIdentifierOrKeyword() {
    int startLine = line_;
    int startCol = col_;
    size_t start = pos_;
    while (!isAtEnd() && isIdentCont(peek())) advance();
    std::string text = source_.substr(start, pos_ - start);

    Token t;
    auto it = keywords().find(text);
    t.type = it != keywords().end() ? it->second : TokenType::Identifier;
    t.text = std::move(text);
    t.line = startLine;
    t.col = startCol;
    return t;
}

Token Lexer::lexNumber() {
    int startLine = line_;
    int startCol = col_;
    size_t start = pos_;
    bool isDouble = false;

    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) advance();
    if (!isAtEnd() && peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        isDouble = true;
        advance();  // consume '.'
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) advance();
    }

    Token t;
    t.type = isDouble ? TokenType::DoubleLiteral : TokenType::IntLiteral;
    t.text = source_.substr(start, pos_ - start);
    t.line = startLine;
    t.col = startCol;
    return t;
}

Token Lexer::lexString() {
    int startLine = line_;
    int startCol = col_;
    size_t start = pos_;
    advance();  // opening quote

    std::string decoded;
    bool closed = false;
    while (!isAtEnd()) {
        char c = peek();
        if (c == '"') {
            advance();
            closed = true;
            break;
        }
        if (c == '\n') break;  // unterminated: no multi-line string literals
        if (c == '\\') {
            advance();
            if (isAtEnd()) break;
            char esc = advance();
            switch (esc) {
                case 'n': decoded += '\n'; break;
                case 't': decoded += '\t'; break;
                case '"': decoded += '"'; break;
                case '\\': decoded += '\\'; break;
                default:
                    decoded += esc;
                    break;
            }
            continue;
        }
        decoded += advance();
    }

    if (!closed) {
        return errorToken("unterminated string literal", startLine, startCol,
                           source_.substr(start, pos_ - start));
    }

    Token t;
    t.type = TokenType::StringLiteral;
    t.text = source_.substr(start, pos_ - start);
    t.stringValue = decoded;
    t.line = startLine;
    t.col = startCol;
    return t;
}

Token Lexer::next() {
    skipWhitespaceAndComments();

    int startLine = line_;
    int startCol = col_;

    if (isAtEnd()) {
        Token t;
        t.type = TokenType::EndOfFile;
        t.line = startLine;
        t.col = startCol;
        return t;
    }

    char c = peek();

    if (isIdentStart(c)) return lexIdentifierOrKeyword();
    if (std::isdigit(static_cast<unsigned char>(c))) return lexNumber();
    if (c == '"') return lexString();

    auto simple = [&](TokenType type, int len) {
        std::string text = source_.substr(pos_, static_cast<size_t>(len));
        for (int i = 0; i < len; i++) advance();
        Token t;
        t.type = type;
        t.text = std::move(text);
        t.line = startLine;
        t.col = startCol;
        return t;
    };

    switch (c) {
        case '{': return simple(TokenType::LBrace, 1);
        case '}': return simple(TokenType::RBrace, 1);
        case '(': return simple(TokenType::LParen, 1);
        case ')': return simple(TokenType::RParen, 1);
        case '[': return simple(TokenType::LBracket, 1);
        case ']': return simple(TokenType::RBracket, 1);
        case ';': return simple(TokenType::Semicolon, 1);
        case ',': return simple(TokenType::Comma, 1);
        case '.': return simple(TokenType::Dot, 1);
        case '+': return simple(TokenType::Plus, 1);
        case '-': return simple(TokenType::Minus, 1);
        case '*': return simple(TokenType::Star, 1);
        case '/': return simple(TokenType::Slash, 1);
        case '%': return simple(TokenType::Percent, 1);
        case '=': {
            advance();
            if (match('=')) return {TokenType::EqEq, "==", "", startLine, startCol};
            return {TokenType::Assign, "=", "", startLine, startCol};
        }
        case '!': {
            advance();
            if (match('=')) return {TokenType::NotEq, "!=", "", startLine, startCol};
            return {TokenType::Not, "!", "", startLine, startCol};
        }
        case '<': {
            advance();
            if (match('=')) return {TokenType::LtEq, "<=", "", startLine, startCol};
            return {TokenType::Lt, "<", "", startLine, startCol};
        }
        case '>': {
            advance();
            if (match('=')) return {TokenType::GtEq, ">=", "", startLine, startCol};
            return {TokenType::Gt, ">", "", startLine, startCol};
        }
        case '&': {
            advance();
            if (match('&')) return {TokenType::AndAnd, "&&", "", startLine, startCol};
            return errorToken("unexpected character '&'", startLine, startCol, "&");
        }
        case '|': {
            advance();
            if (match('|')) return {TokenType::OrOr, "||", "", startLine, startCol};
            return errorToken("unexpected character '|'", startLine, startCol, "|");
        }
        default: {
            advance();
            return errorToken(std::string("unexpected character '") + c + "'", startLine,
                               startCol, std::string(1, c));
        }
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        Token t = next();
        bool isEof = t.type == TokenType::EndOfFile;
        tokens.push_back(std::move(t));
        if (isEof) break;
    }
    return tokens;
}

}  // namespace jc
