#pragma once

#include <memory>
#include <vector>

#include "ast/Expr.hpp"
#include "ast/Type.hpp"

namespace jc {

enum class StmtKind {
    Block,
    LocalVarDecl,
    If,
    While,
    For,
    Return,
    ExprStmt,
};

struct Stmt {
    explicit Stmt(StmtKind k, int line = 0, int col = 0) : kind(k), line(line), col(col) {}
    virtual ~Stmt() = default;

    StmtKind kind;
    int line;
    int col;
};

using StmtPtr = std::unique_ptr<Stmt>;

struct BlockStmt : Stmt {
    BlockStmt(std::vector<StmtPtr> s, int line, int col)
        : Stmt(StmtKind::Block, line, col), stmts(std::move(s)) {}
    std::vector<StmtPtr> stmts;
};

struct LocalVarDeclStmt : Stmt {
    LocalVarDeclStmt(Type t, std::string n, ExprPtr init_, int line, int col)
        : Stmt(StmtKind::LocalVarDecl, line, col),
          type(std::move(t)),
          name(std::move(n)),
          init(std::move(init_)) {}
    Type type;
    std::string name;
    ExprPtr init;  // nullable
};

struct IfStmt : Stmt {
    IfStmt(ExprPtr c, StmtPtr t, StmtPtr e, int line, int col)
        : Stmt(StmtKind::If, line, col), cond(std::move(c)), thenBranch(std::move(t)), elseBranch(std::move(e)) {}
    ExprPtr cond;
    StmtPtr thenBranch;
    StmtPtr elseBranch;  // nullable
};

struct WhileStmt : Stmt {
    WhileStmt(ExprPtr c, StmtPtr b, int line, int col)
        : Stmt(StmtKind::While, line, col), cond(std::move(c)), body(std::move(b)) {}
    ExprPtr cond;
    StmtPtr body;
};

struct ForStmt : Stmt {
    ForStmt(StmtPtr init_, ExprPtr cond_, ExprPtr update_, StmtPtr body_, int line, int col)
        : Stmt(StmtKind::For, line, col),
          init(std::move(init_)),
          cond(std::move(cond_)),
          update(std::move(update_)),
          body(std::move(body_)) {}
    StmtPtr init;    // nullable; LocalVarDeclStmt or ExprStmt
    ExprPtr cond;    // nullable
    ExprPtr update;  // nullable
    StmtPtr body;
};

struct ReturnStmt : Stmt {
    ReturnStmt(ExprPtr v, int line, int col) : Stmt(StmtKind::Return, line, col), value(std::move(v)) {}
    ExprPtr value;  // nullable
};

struct ExprStmt : Stmt {
    ExprStmt(ExprPtr e, int line, int col) : Stmt(StmtKind::ExprStmt, line, col), expr(std::move(e)) {}
    ExprPtr expr;
};

}  // namespace jc
