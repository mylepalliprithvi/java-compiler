#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ast/Decl.hpp"
#include "lexer/Token.hpp"

namespace jc {

struct ParseError {
    int line;
    int col;
    std::string message;
};

// Recursive-descent parser (Pratt/precedence-climbing for expressions) over
// a fully-tokenized source. Never throws out of parseCompilationUnit():
// syntax errors are recorded via errors() and the parser recovers to the
// next statement/member boundary (panic-mode) so multiple errors can be
// reported per file.
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    std::unique_ptr<CompilationUnit> parseCompilationUnit();
    const std::vector<ParseError>& errors() const { return errors_; }

private:
    // Internal control-flow signal used to unwind to a recovery point after
    // a syntax error; always caught within this class.
    struct ParseException {};

    // --- declarations ---
    ClassDecl parseClassDecl();
    void parseMember(const std::string& className, std::vector<FieldDecl>& fields,
                      std::vector<ConstructorDecl>& constructors, std::vector<MethodDecl>& methods);
    std::vector<Param> parseParamList();
    Param parseParam();
    Type parseType();

    // --- statements ---
    std::unique_ptr<BlockStmt> parseBlock();
    StmtPtr parseStatement();
    StmtPtr parseLocalVarDecl();
    StmtPtr parseIf();
    StmtPtr parseWhile();
    StmtPtr parseFor();
    StmtPtr parseReturn();
    StmtPtr parseExprStatement();
    bool isLocalVarDeclStart() const;

    // --- expressions (precedence climbing) ---
    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseOr();
    ExprPtr parseAnd();
    ExprPtr parseEquality();
    ExprPtr parseRelational();
    ExprPtr parseAdditive();
    ExprPtr parseMultiplicative();
    ExprPtr parseUnary();
    ExprPtr parsePostfix();
    ExprPtr parsePrimary();
    std::vector<ExprPtr> parseArgs();

    // --- token stream helpers ---
    const Token& current() const;
    const Token& peek(int offset) const;
    bool check(TokenType type) const;
    bool isAtEnd() const;
    const Token& advance();
    bool match(TokenType type);
    const Token& expect(TokenType type, const std::string& what);
    [[noreturn]] void error(const std::string& message);
    [[noreturn]] void errorAt(const Token& tok, const std::string& message);
    void synchronize();

    std::vector<Token> tokens_;
    size_t pos_ = 0;
    std::vector<ParseError> errors_;
};

}  // namespace jc
