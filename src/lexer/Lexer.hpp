#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "lexer/Token.hpp"

namespace jc {

struct LexError {
    int line;
    int col;
    std::string message;
};

// Hand-written scanner for the v0 subset (see docs/subset-v0.md). Never
// throws: unrecognized input produces an Error token and a recorded
// diagnostic, so callers can keep scanning to report multiple errors at
// once.
class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> tokenize();
    const std::vector<LexError>& errors() const { return errors_; }

private:
    Token next();
    void skipWhitespaceAndComments();
    Token lexIdentifierOrKeyword();
    Token lexNumber();
    Token lexString();
    Token errorToken(const std::string& message, int line, int col, std::string text);

    char peek(int offset = 0) const;
    char advance();
    bool match(char expected);
    bool isAtEnd() const;

    std::string source_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
    std::vector<LexError> errors_;
};

}  // namespace jc
