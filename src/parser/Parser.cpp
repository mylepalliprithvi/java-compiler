#include "parser/Parser.hpp"

#include <string>
#include <utility>

namespace jc {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

// --- token stream helpers ---

const Token& Parser::current() const { return tokens_[pos_]; }

const Token& Parser::peek(int offset) const {
    size_t idx = pos_ + static_cast<size_t>(offset);
    if (idx >= tokens_.size()) idx = tokens_.size() - 1;
    return tokens_[idx];
}

bool Parser::isAtEnd() const { return current().type == TokenType::EndOfFile; }

const Token& Parser::advance() {
    const Token& tok = current();
    if (!isAtEnd()) pos_++;
    return tok;
}

bool Parser::check(TokenType type) const { return current().type == type; }

bool Parser::match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

const Token& Parser::expect(TokenType type, const std::string& what) {
    if (check(type)) return advance();
    errorAt(current(), "expected " + what + " but got '" + current().text + "'");
}

void Parser::error(const std::string& message) { errorAt(current(), message); }

void Parser::errorAt(const Token& tok, const std::string& message) {
    errors_.push_back({tok.line, tok.col, message});
    throw ParseException{};
}

void Parser::synchronize() {
    while (!isAtEnd()) {
        if (current().type == TokenType::Semicolon) {
            advance();
            return;
        }
        if (current().type == TokenType::RBrace) return;
        advance();
    }
}

// --- top level ---

std::unique_ptr<CompilationUnit> Parser::parseCompilationUnit() {
    auto unit = std::make_unique<CompilationUnit>();
    try {
        unit->classDecl = parseClassDecl();
    } catch (const ParseException&) {
        return nullptr;
    }
    if (!isAtEnd()) {
        errors_.push_back(
            {current().line, current().col, "unexpected trailing input after class body"});
    }
    return unit;
}

ClassDecl Parser::parseClassDecl() {
    ClassDecl decl;
    decl.line = current().line;
    decl.col = current().col;
    decl.isPublic = match(TokenType::KwPublic);
    expect(TokenType::KwClass, "'class'");
    decl.name = expect(TokenType::Identifier, "class name").text;
    if (match(TokenType::KwExtends)) {
        decl.superclass = expect(TokenType::Identifier, "superclass name").text;
    }
    expect(TokenType::LBrace, "'{'");
    while (!check(TokenType::RBrace) && !isAtEnd()) {
        try {
            parseMember(decl.name, decl.fields, decl.constructors, decl.methods);
        } catch (const ParseException&) {
            synchronize();
        }
    }
    expect(TokenType::RBrace, "'}'");
    return decl;
}

void Parser::parseMember(const std::string& className, std::vector<FieldDecl>& fields,
                          std::vector<ConstructorDecl>& constructors,
                          std::vector<MethodDecl>& methods) {
    int line = current().line;
    int col = current().col;
    bool sawPrivate = false;
    bool sawStatic = false;
    for (;;) {
        if (match(TokenType::KwPublic)) continue;
        if (match(TokenType::KwPrivate)) {
            sawPrivate = true;
            continue;
        }
        if (match(TokenType::KwStatic)) {
            sawStatic = true;
            continue;
        }
        break;
    }
    bool isPublic = !sawPrivate;

    if (check(TokenType::Identifier) && current().text == className &&
        peek(1).type == TokenType::LParen) {
        advance();  // constructor name
        ConstructorDecl ctor;
        ctor.isPublic = isPublic;
        ctor.line = line;
        ctor.col = col;
        ctor.params = parseParamList();
        ctor.body = parseBlock();
        constructors.push_back(std::move(ctor));
        return;
    }

    Type type = parseType();
    std::string name = expect(TokenType::Identifier, "member name").text;

    if (check(TokenType::LParen)) {
        MethodDecl method;
        method.isPublic = isPublic;
        method.isStatic = sawStatic;
        method.returnType = type;
        method.name = name;
        method.line = line;
        method.col = col;
        method.params = parseParamList();
        method.body = parseBlock();
        methods.push_back(std::move(method));
        return;
    }

    expect(TokenType::Semicolon, "';'");
    FieldDecl field;
    field.type = type;
    field.name = name;
    field.isPublic = isPublic;
    field.line = line;
    field.col = col;
    fields.push_back(std::move(field));
}

std::vector<Param> Parser::parseParamList() {
    expect(TokenType::LParen, "'('");
    std::vector<Param> params;
    if (!check(TokenType::RParen)) {
        params.push_back(parseParam());
        while (match(TokenType::Comma)) params.push_back(parseParam());
    }
    expect(TokenType::RParen, "')'");
    return params;
}

Param Parser::parseParam() {
    Type t = parseType();
    std::string name = expect(TokenType::Identifier, "parameter name").text;
    return Param{t, name};
}

Type Parser::parseType() {
    if (match(TokenType::KwInt)) return Type::primitive(Type::Kind::Int);
    if (match(TokenType::KwBoolean)) return Type::primitive(Type::Kind::Boolean);
    if (match(TokenType::KwDouble)) return Type::primitive(Type::Kind::Double);
    if (match(TokenType::KwVoid)) return Type::primitive(Type::Kind::Void);
    if (check(TokenType::Identifier)) {
        std::string name = advance().text;
        if (match(TokenType::LBracket)) {
            expect(TokenType::RBracket, "']'");
            return Type::arrayOfClassRef(name);
        }
        return Type::classRef(name);
    }
    error("expected a type");
}

// --- statements ---

bool Parser::isLocalVarDeclStart() const {
    TokenType t0 = current().type;
    if (t0 == TokenType::KwInt || t0 == TokenType::KwBoolean || t0 == TokenType::KwDouble)
        return true;
    if (t0 != TokenType::Identifier) return false;
    int idx = 1;
    if (peek(1).type == TokenType::LBracket && peek(2).type == TokenType::RBracket) idx = 3;
    return peek(idx).type == TokenType::Identifier;
}

std::unique_ptr<BlockStmt> Parser::parseBlock() {
    int line = current().line;
    int col = current().col;
    expect(TokenType::LBrace, "'{'");
    std::vector<StmtPtr> stmts;
    while (!check(TokenType::RBrace) && !isAtEnd()) {
        try {
            stmts.push_back(parseStatement());
        } catch (const ParseException&) {
            synchronize();
        }
    }
    expect(TokenType::RBrace, "'}'");
    return std::make_unique<BlockStmt>(std::move(stmts), line, col);
}

StmtPtr Parser::parseStatement() {
    if (check(TokenType::LBrace)) return parseBlock();
    if (check(TokenType::KwIf)) return parseIf();
    if (check(TokenType::KwWhile)) return parseWhile();
    if (check(TokenType::KwFor)) return parseFor();
    if (check(TokenType::KwReturn)) return parseReturn();
    if (isLocalVarDeclStart()) return parseLocalVarDecl();
    return parseExprStatement();
}

StmtPtr Parser::parseLocalVarDecl() {
    int line = current().line;
    int col = current().col;
    Type type = parseType();
    std::string name = expect(TokenType::Identifier, "variable name").text;
    ExprPtr init;
    if (match(TokenType::Assign)) init = parseExpression();
    expect(TokenType::Semicolon, "';'");
    return std::make_unique<LocalVarDeclStmt>(std::move(type), std::move(name), std::move(init),
                                               line, col);
}

StmtPtr Parser::parseIf() {
    int line = current().line;
    int col = current().col;
    advance();  // 'if'
    expect(TokenType::LParen, "'('");
    ExprPtr cond = parseExpression();
    expect(TokenType::RParen, "')'");
    StmtPtr thenBranch = parseStatement();
    StmtPtr elseBranch;
    if (match(TokenType::KwElse)) elseBranch = parseStatement();
    return std::make_unique<IfStmt>(std::move(cond), std::move(thenBranch), std::move(elseBranch),
                                     line, col);
}

StmtPtr Parser::parseWhile() {
    int line = current().line;
    int col = current().col;
    advance();  // 'while'
    expect(TokenType::LParen, "'('");
    ExprPtr cond = parseExpression();
    expect(TokenType::RParen, "')'");
    StmtPtr body = parseStatement();
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body), line, col);
}

StmtPtr Parser::parseFor() {
    int line = current().line;
    int col = current().col;
    advance();  // 'for'
    expect(TokenType::LParen, "'('");

    StmtPtr init;
    if (check(TokenType::Semicolon)) {
        advance();
    } else if (isLocalVarDeclStart()) {
        init = parseLocalVarDecl();  // consumes trailing ';'
    } else {
        int eline = current().line;
        int ecol = current().col;
        ExprPtr e = parseExpression();
        expect(TokenType::Semicolon, "';'");
        init = std::make_unique<ExprStmt>(std::move(e), eline, ecol);
    }

    ExprPtr cond;
    if (!check(TokenType::Semicolon)) cond = parseExpression();
    expect(TokenType::Semicolon, "';'");

    ExprPtr update;
    if (!check(TokenType::RParen)) update = parseExpression();
    expect(TokenType::RParen, "')'");

    StmtPtr body = parseStatement();
    return std::make_unique<ForStmt>(std::move(init), std::move(cond), std::move(update),
                                      std::move(body), line, col);
}

StmtPtr Parser::parseReturn() {
    int line = current().line;
    int col = current().col;
    advance();  // 'return'
    ExprPtr value;
    if (!check(TokenType::Semicolon)) value = parseExpression();
    expect(TokenType::Semicolon, "';'");
    return std::make_unique<ReturnStmt>(std::move(value), line, col);
}

StmtPtr Parser::parseExprStatement() {
    int line = current().line;
    int col = current().col;
    ExprPtr e = parseExpression();
    expect(TokenType::Semicolon, "';'");
    return std::make_unique<ExprStmt>(std::move(e), line, col);
}

// --- expressions ---

ExprPtr Parser::parseExpression() { return parseAssignment(); }

ExprPtr Parser::parseAssignment() {
    ExprPtr left = parseOr();
    if (check(TokenType::Assign)) {
        Token eqTok = current();
        advance();
        if (left->kind != ExprKind::Name && left->kind != ExprKind::FieldAccess) {
            errorAt(eqTok, "invalid assignment target");
        }
        ExprPtr right = parseAssignment();
        return std::make_unique<AssignExpr>(std::move(left), std::move(right), eqTok.line,
                                             eqTok.col);
    }
    return left;
}

ExprPtr Parser::parseOr() {
    ExprPtr left = parseAnd();
    while (check(TokenType::OrOr)) {
        Token op = current();
        advance();
        ExprPtr right = parseAnd();
        left = std::make_unique<BinaryExpr>(BinaryOp::Or, std::move(left), std::move(right),
                                             op.line, op.col);
    }
    return left;
}

ExprPtr Parser::parseAnd() {
    ExprPtr left = parseEquality();
    while (check(TokenType::AndAnd)) {
        Token op = current();
        advance();
        ExprPtr right = parseEquality();
        left = std::make_unique<BinaryExpr>(BinaryOp::And, std::move(left), std::move(right),
                                             op.line, op.col);
    }
    return left;
}

ExprPtr Parser::parseEquality() {
    ExprPtr left = parseRelational();
    while (check(TokenType::EqEq) || check(TokenType::NotEq)) {
        Token op = current();
        advance();
        BinaryOp bop = op.type == TokenType::EqEq ? BinaryOp::Eq : BinaryOp::NotEq;
        ExprPtr right = parseRelational();
        left = std::make_unique<BinaryExpr>(bop, std::move(left), std::move(right), op.line,
                                             op.col);
    }
    return left;
}

ExprPtr Parser::parseRelational() {
    ExprPtr left = parseAdditive();
    while (check(TokenType::Lt) || check(TokenType::LtEq) || check(TokenType::Gt) ||
           check(TokenType::GtEq)) {
        Token op = current();
        advance();
        BinaryOp bop;
        switch (op.type) {
            case TokenType::Lt: bop = BinaryOp::Lt; break;
            case TokenType::LtEq: bop = BinaryOp::LtEq; break;
            case TokenType::Gt: bop = BinaryOp::Gt; break;
            default: bop = BinaryOp::GtEq; break;
        }
        ExprPtr right = parseAdditive();
        left = std::make_unique<BinaryExpr>(bop, std::move(left), std::move(right), op.line,
                                             op.col);
    }
    return left;
}

ExprPtr Parser::parseAdditive() {
    ExprPtr left = parseMultiplicative();
    while (check(TokenType::Plus) || check(TokenType::Minus)) {
        Token op = current();
        advance();
        BinaryOp bop = op.type == TokenType::Plus ? BinaryOp::Add : BinaryOp::Sub;
        ExprPtr right = parseMultiplicative();
        left = std::make_unique<BinaryExpr>(bop, std::move(left), std::move(right), op.line,
                                             op.col);
    }
    return left;
}

ExprPtr Parser::parseMultiplicative() {
    ExprPtr left = parseUnary();
    while (check(TokenType::Star) || check(TokenType::Slash) || check(TokenType::Percent)) {
        Token op = current();
        advance();
        BinaryOp bop = op.type == TokenType::Star
                            ? BinaryOp::Mul
                            : (op.type == TokenType::Slash ? BinaryOp::Div : BinaryOp::Mod);
        ExprPtr right = parseUnary();
        left = std::make_unique<BinaryExpr>(bop, std::move(left), std::move(right), op.line,
                                             op.col);
    }
    return left;
}

ExprPtr Parser::parseUnary() {
    if (check(TokenType::Not) || check(TokenType::Minus)) {
        Token op = current();
        advance();
        ExprPtr operand = parseUnary();
        UnaryOp uop = op.type == TokenType::Not ? UnaryOp::Not : UnaryOp::Neg;
        return std::make_unique<UnaryExpr>(uop, std::move(operand), op.line, op.col);
    }
    return parsePostfix();
}

ExprPtr Parser::parsePostfix() {
    ExprPtr expr = parsePrimary();
    for (;;) {
        if (match(TokenType::Dot)) {
            Token nameTok = expect(TokenType::Identifier, "member name after '.'");
            if (check(TokenType::LParen)) {
                std::vector<ExprPtr> args = parseArgs();
                expr = std::make_unique<MethodCallExpr>(std::move(expr), nameTok.text,
                                                          std::move(args), nameTok.line,
                                                          nameTok.col);
            } else {
                expr = std::make_unique<FieldAccessExpr>(std::move(expr), nameTok.text,
                                                           nameTok.line, nameTok.col);
            }
        } else {
            break;
        }
    }
    return expr;
}

std::vector<ExprPtr> Parser::parseArgs() {
    expect(TokenType::LParen, "'('");
    std::vector<ExprPtr> args;
    if (!check(TokenType::RParen)) {
        args.push_back(parseExpression());
        while (match(TokenType::Comma)) args.push_back(parseExpression());
    }
    expect(TokenType::RParen, "')'");
    return args;
}

ExprPtr Parser::parsePrimary() {
    Token tok = current();

    if (match(TokenType::IntLiteral)) {
        return std::make_unique<IntLiteralExpr>(std::stoll(tok.text), tok.line, tok.col);
    }
    if (match(TokenType::DoubleLiteral)) {
        return std::make_unique<DoubleLiteralExpr>(std::stod(tok.text), tok.line, tok.col);
    }
    if (match(TokenType::KwTrue)) {
        return std::make_unique<BoolLiteralExpr>(true, tok.line, tok.col);
    }
    if (match(TokenType::KwFalse)) {
        return std::make_unique<BoolLiteralExpr>(false, tok.line, tok.col);
    }
    if (match(TokenType::StringLiteral)) {
        return std::make_unique<StringLiteralExpr>(tok.stringValue, tok.line, tok.col);
    }
    if (match(TokenType::KwThis)) {
        return std::make_unique<ThisExpr>(tok.line, tok.col);
    }
    if (match(TokenType::KwNew)) {
        Token nameTok = expect(TokenType::Identifier, "class name after 'new'");
        std::vector<ExprPtr> args = parseArgs();
        return std::make_unique<NewExpr>(nameTok.text, std::move(args), tok.line, tok.col);
    }
    if (match(TokenType::Identifier)) {
        if (check(TokenType::LParen)) {
            std::vector<ExprPtr> args = parseArgs();
            return std::make_unique<MethodCallExpr>(nullptr, tok.text, std::move(args), tok.line,
                                                      tok.col);
        }
        return std::make_unique<NameExpr>(tok.text, tok.line, tok.col);
    }
    if (match(TokenType::LParen)) {
        ExprPtr inner = parseExpression();
        expect(TokenType::RParen, "')'");
        return inner;
    }

    errorAt(tok, "expected an expression but got '" + tok.text + "'");
}

}  // namespace jc
