#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ast/Type.hpp"

namespace jc {

enum class ExprKind {
    IntLiteral,
    DoubleLiteral,
    BoolLiteral,
    StringLiteral,
    Name,
    This,
    FieldAccess,
    MethodCall,
    New,
    Assign,
    Unary,
    Binary,
    Error,
};

struct Expr {
    explicit Expr(ExprKind k, int line = 0, int col = 0) : kind(k), line(line), col(col) {}
    virtual ~Expr() = default;

    ExprKind kind;
    int line;
    int col;

    // Filled in by SemanticAnalyzer; codegen trusts these without re-deriving
    // them. typeResolved stays false for nodes sema couldn't type (e.g. one
    // that already had a parse error), so codegen never runs over them.
    Type resolvedType = Type::primitive(Type::Kind::Void);
    bool typeResolved = false;
};

using ExprPtr = std::unique_ptr<Expr>;

struct IntLiteralExpr : Expr {
    IntLiteralExpr(long long v, int line, int col) : Expr(ExprKind::IntLiteral, line, col), value(v) {}
    long long value;
};

struct DoubleLiteralExpr : Expr {
    DoubleLiteralExpr(double v, int line, int col) : Expr(ExprKind::DoubleLiteral, line, col), value(v) {}
    double value;
};

struct BoolLiteralExpr : Expr {
    BoolLiteralExpr(bool v, int line, int col) : Expr(ExprKind::BoolLiteral, line, col), value(v) {}
    bool value;
};

struct StringLiteralExpr : Expr {
    StringLiteralExpr(std::string v, int line, int col)
        : Expr(ExprKind::StringLiteral, line, col), value(std::move(v)) {}
    std::string value;
};

// A bare identifier reference (local, param, or field) — resolved during
// semantic analysis, not by the parser.
struct NameExpr : Expr {
    enum class RefKind { Unresolved, Local, Field };

    NameExpr(std::string n, int line, int col) : Expr(ExprKind::Name, line, col), name(std::move(n)) {}
    std::string name;

    RefKind refKind = RefKind::Unresolved;
    int slot = -1;  // valid only when refKind == Local
};

struct ThisExpr : Expr {
    ThisExpr(int line, int col) : Expr(ExprKind::This, line, col) {}
};

// target.name
struct FieldAccessExpr : Expr {
    FieldAccessExpr(ExprPtr t, std::string n, int line, int col)
        : Expr(ExprKind::FieldAccess, line, col), target(std::move(t)), name(std::move(n)) {}
    ExprPtr target;
    std::string name;
};

// target.name(args) — target is null for an unqualified call (implicit this).
struct MethodCallExpr : Expr {
    enum class CallKind { Unresolved, UserMethod, PrintlnSpecial };

    MethodCallExpr(ExprPtr t, std::string n, std::vector<ExprPtr> a, int line, int col)
        : Expr(ExprKind::MethodCall, line, col),
          target(std::move(t)),
          name(std::move(n)),
          args(std::move(a)) {}
    ExprPtr target;
    std::string name;
    std::vector<ExprPtr> args;

    // PrintlnSpecial means `target` was recognized as the fixed
    // `System.out.println(...)` pattern and was NOT itself resolved as a
    // general expression (see SemanticAnalyzer) — java.lang.System has no
    // symbol-table entry in v0.
    CallKind callKind = CallKind::Unresolved;
};

struct NewExpr : Expr {
    NewExpr(std::string cn, std::vector<ExprPtr> a, int line, int col)
        : Expr(ExprKind::New, line, col), className(std::move(cn)), args(std::move(a)) {}
    std::string className;
    std::vector<ExprPtr> args;
};

// target = value; target must be a NameExpr or FieldAccessExpr (checked by
// the parser at construction time via makeAssign()).
struct AssignExpr : Expr {
    AssignExpr(ExprPtr t, ExprPtr v, int line, int col)
        : Expr(ExprKind::Assign, line, col), target(std::move(t)), value(std::move(v)) {}
    ExprPtr target;
    ExprPtr value;
};

enum class UnaryOp { Neg, Not };

struct UnaryExpr : Expr {
    UnaryExpr(UnaryOp o, ExprPtr operand_, int line, int col)
        : Expr(ExprKind::Unary, line, col), op(o), operand(std::move(operand_)) {}
    UnaryOp op;
    ExprPtr operand;
};

enum class BinaryOp {
    Add, Sub, Mul, Div, Mod,
    Lt, LtEq, Gt, GtEq, Eq, NotEq,
    And, Or,
};

struct BinaryExpr : Expr {
    BinaryExpr(BinaryOp o, ExprPtr l, ExprPtr r, int line, int col)
        : Expr(ExprKind::Binary, line, col), op(o), left(std::move(l)), right(std::move(r)) {}
    BinaryOp op;
    ExprPtr left;
    ExprPtr right;
};

// Placeholder produced when the parser cannot recover an expression at all
// (e.g. `1 + ;`); lets parsing continue for further diagnostics.
struct ErrorExpr : Expr {
    ErrorExpr(int line, int col) : Expr(ExprKind::Error, line, col) {}
};

}  // namespace jc
