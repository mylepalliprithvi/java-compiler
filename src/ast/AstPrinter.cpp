#include "ast/AstPrinter.hpp"

#include <sstream>

namespace jc {

namespace {

void printExpr(std::ostringstream& out, const Expr* e);
void printStmt(std::ostringstream& out, const Stmt* s);

const char* binOpStr(BinaryOp op) {
    switch (op) {
        case BinaryOp::Add: return "+";
        case BinaryOp::Sub: return "-";
        case BinaryOp::Mul: return "*";
        case BinaryOp::Div: return "/";
        case BinaryOp::Mod: return "%";
        case BinaryOp::Lt: return "<";
        case BinaryOp::LtEq: return "<=";
        case BinaryOp::Gt: return ">";
        case BinaryOp::GtEq: return ">=";
        case BinaryOp::Eq: return "==";
        case BinaryOp::NotEq: return "!=";
        case BinaryOp::And: return "&&";
        case BinaryOp::Or: return "||";
    }
    return "?";
}

void printExpr(std::ostringstream& out, const Expr* e) {
    if (e == nullptr) {
        out << "none";
        return;
    }
    switch (e->kind) {
        case ExprKind::IntLiteral:
            out << static_cast<const IntLiteralExpr*>(e)->value;
            return;
        case ExprKind::DoubleLiteral:
            out << static_cast<const DoubleLiteralExpr*>(e)->value;
            return;
        case ExprKind::BoolLiteral:
            out << (static_cast<const BoolLiteralExpr*>(e)->value ? "true" : "false");
            return;
        case ExprKind::StringLiteral:
            out << '"' << static_cast<const StringLiteralExpr*>(e)->value << '"';
            return;
        case ExprKind::Name:
            out << static_cast<const NameExpr*>(e)->name;
            return;
        case ExprKind::This:
            out << "this";
            return;
        case ExprKind::FieldAccess: {
            const auto* fa = static_cast<const FieldAccessExpr*>(e);
            out << "(field-access ";
            printExpr(out, fa->target.get());
            out << " " << fa->name << ")";
            return;
        }
        case ExprKind::MethodCall: {
            const auto* mc = static_cast<const MethodCallExpr*>(e);
            out << "(call ";
            printExpr(out, mc->target.get());
            out << " " << mc->name;
            for (const auto& a : mc->args) {
                out << " ";
                printExpr(out, a.get());
            }
            out << ")";
            return;
        }
        case ExprKind::New: {
            const auto* n = static_cast<const NewExpr*>(e);
            out << "(new " << n->className;
            for (const auto& a : n->args) {
                out << " ";
                printExpr(out, a.get());
            }
            out << ")";
            return;
        }
        case ExprKind::Assign: {
            const auto* a = static_cast<const AssignExpr*>(e);
            out << "(assign ";
            printExpr(out, a->target.get());
            out << " ";
            printExpr(out, a->value.get());
            out << ")";
            return;
        }
        case ExprKind::Unary: {
            const auto* u = static_cast<const UnaryExpr*>(e);
            out << "(unary " << (u->op == UnaryOp::Neg ? "-" : "!") << " ";
            printExpr(out, u->operand.get());
            out << ")";
            return;
        }
        case ExprKind::Binary: {
            const auto* b = static_cast<const BinaryExpr*>(e);
            out << "(binary " << binOpStr(b->op) << " ";
            printExpr(out, b->left.get());
            out << " ";
            printExpr(out, b->right.get());
            out << ")";
            return;
        }
        case ExprKind::Error:
            out << "(error-expr)";
            return;
    }
}

void printStmt(std::ostringstream& out, const Stmt* s) {
    if (s == nullptr) {
        out << "none";
        return;
    }
    switch (s->kind) {
        case StmtKind::Block: {
            const auto* b = static_cast<const BlockStmt*>(s);
            out << "(block";
            for (const auto& st : b->stmts) {
                out << " ";
                printStmt(out, st.get());
            }
            out << ")";
            return;
        }
        case StmtKind::LocalVarDecl: {
            const auto* d = static_cast<const LocalVarDeclStmt*>(s);
            out << "(local " << toString(d->type) << " " << d->name << " ";
            printExpr(out, d->init.get());
            out << ")";
            return;
        }
        case StmtKind::If: {
            const auto* i = static_cast<const IfStmt*>(s);
            out << "(if ";
            printExpr(out, i->cond.get());
            out << " ";
            printStmt(out, i->thenBranch.get());
            out << " ";
            printStmt(out, i->elseBranch.get());
            out << ")";
            return;
        }
        case StmtKind::While: {
            const auto* w = static_cast<const WhileStmt*>(s);
            out << "(while ";
            printExpr(out, w->cond.get());
            out << " ";
            printStmt(out, w->body.get());
            out << ")";
            return;
        }
        case StmtKind::For: {
            const auto* f = static_cast<const ForStmt*>(s);
            out << "(for ";
            printStmt(out, f->init.get());
            out << " ";
            printExpr(out, f->cond.get());
            out << " ";
            printExpr(out, f->update.get());
            out << " ";
            printStmt(out, f->body.get());
            out << ")";
            return;
        }
        case StmtKind::Return: {
            const auto* r = static_cast<const ReturnStmt*>(s);
            out << "(return ";
            printExpr(out, r->value.get());
            out << ")";
            return;
        }
        case StmtKind::ExprStmt: {
            const auto* e = static_cast<const ExprStmt*>(s);
            out << "(expr-stmt ";
            printExpr(out, e->expr.get());
            out << ")";
            return;
        }
    }
}

void printParams(std::ostringstream& out, const std::vector<Param>& params) {
    out << "(params";
    for (const auto& p : params) {
        out << " (param " << toString(p.type) << " " << p.name << ")";
    }
    out << ")";
}

}  // namespace

std::string printAst(const CompilationUnit& unit) {
    std::ostringstream out;
    const ClassDecl& c = unit.classDecl;

    out << "(class " << c.name;
    if (c.superclass) out << " extends " << *c.superclass;

    out << " (fields";
    for (const auto& f : c.fields) out << " (field " << toString(f.type) << " " << f.name << ")";
    out << ")";

    out << " (ctors";
    for (const auto& ctor : c.constructors) {
        out << " (ctor ";
        printParams(out, ctor.params);
        out << " ";
        printStmt(out, ctor.body.get());
        out << ")";
    }
    out << ")";

    out << " (methods";
    for (const auto& m : c.methods) {
        out << " (method";
        if (m.isStatic) out << " static";
        out << " " << toString(m.returnType) << " " << m.name << " ";
        printParams(out, m.params);
        out << " ";
        printStmt(out, m.body.get());
        out << ")";
    }
    out << ")";

    out << ")";
    return out.str();
}

}  // namespace jc
