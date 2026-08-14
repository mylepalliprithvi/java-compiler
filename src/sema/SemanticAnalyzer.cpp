#include "sema/SemanticAnalyzer.hpp"

#include <string>

namespace jc {

void SemanticAnalyzer::error(int line, int col, std::string message) {
    errors_.push_back({line, col, std::move(message)});
}

int SemanticAnalyzer::slotSize(const Type& type) { return type.kind == Type::Kind::Double ? 2 : 1; }

bool SemanticAnalyzer::isNumeric(const Type& t) {
    return t.kind == Type::Kind::Int || t.kind == Type::Kind::Double;
}

bool SemanticAnalyzer::assignable(const Type& target, const Type& value) {
    if (target.kind == value.kind) {
        if (target.kind == Type::Kind::ClassRef || target.kind == Type::Kind::ArrayOfClassRef) {
            return target.className == value.className;
        }
        return true;
    }
    // int -> double widening only.
    return target.kind == Type::Kind::Double && value.kind == Type::Kind::Int;
}

void SemanticAnalyzer::pushScope() { scopes_.emplace_back(); }
void SemanticAnalyzer::popScope() { scopes_.pop_back(); }

SemanticAnalyzer::LocalInfo* SemanticAnalyzer::lookupLocal(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}

SemanticAnalyzer::LocalInfo* SemanticAnalyzer::declareLocal(const std::string& name, Type type,
                                                             int line, int col) {
    Scope& scope = scopes_.back();
    if (scope.count(name)) {
        error(line, col, "'" + name + "' is already declared in this scope");
        return nullptr;
    }
    int slot = currentMethod_->nextSlot;
    currentMethod_->nextSlot += slotSize(type);
    auto [it, inserted] = scope.emplace(name, LocalInfo{type, slot});
    return &it->second;
}

void SemanticAnalyzer::checkNoArray(const Type& type, int line, int col, const char* what) {
    if (type.kind == Type::Kind::ArrayOfClassRef) {
        error(line, col, std::string("arrays are not supported yet (") + what + ")");
    }
}

void SemanticAnalyzer::requireBoolean(const Expr& e, const char* context) {
    if (e.typeResolved && e.resolvedType.kind != Type::Kind::Boolean) {
        error(e.line, e.col, std::string(context) + " must be boolean");
    }
}

bool SemanticAnalyzer::isPrintlnPattern(const MethodCallExpr& call) const {
    if (call.name != "println" || call.args.size() != 1 || !call.target) return false;
    if (call.target->kind != ExprKind::FieldAccess) return false;
    const auto& fa = static_cast<const FieldAccessExpr&>(*call.target);
    if (fa.name != "out" || !fa.target || fa.target->kind != ExprKind::Name) return false;
    const auto& n = static_cast<const NameExpr&>(*fa.target);
    return n.name == "System";
}

bool SemanticAnalyzer::analyze(CompilationUnit& unit) {
    ClassDecl& c = unit.classDecl;
    classTable_.build(
        c, [this](int line, int col, std::string msg) { error(line, col, std::move(msg)); });

    for (auto& f : c.fields) checkNoArray(f.type, f.line, f.col, "field");

    for (auto& ctor : c.constructors) analyzeConstructor(ctor);
    for (auto& m : c.methods) analyzeMethod(m);

    return errors_.empty();
}

void SemanticAnalyzer::analyzeMethod(MethodDecl& method) {
    checkNoArray(method.returnType, method.line, method.col, "return type");

    MethodContext ctx;
    ctx.isStatic = method.isStatic;
    ctx.returnType = method.returnType;
    ctx.nextSlot = method.isStatic ? 0 : 1;
    currentMethod_ = &ctx;

    scopes_.clear();
    pushScope();

    bool isMainEntry = method.isStatic && method.name == "main" && method.params.size() == 1 &&
                        method.params[0].type.kind == Type::Kind::ArrayOfClassRef &&
                        method.params[0].type.className == "String";

    for (size_t i = 0; i < method.params.size(); i++) {
        Param& p = method.params[i];
        bool skipArrayCheck = isMainEntry && i == 0;
        if (!skipArrayCheck) checkNoArray(p.type, method.line, method.col, "parameter");
        LocalInfo* info = declareLocal(p.name, p.type, method.line, method.col);
        if (info) p.slot = info->slot;
    }

    if (method.body) {
        // Body statements share the params' scope frame directly (rather
        // than nesting another one via analyzeBlock), matching Java: a
        // local in the body's top level cannot shadow a parameter.
        for (auto& s : method.body->stmts) analyzeStmt(*s);
    }

    method.maxLocals = ctx.nextSlot;
    popScope();
    currentMethod_ = nullptr;
}

void SemanticAnalyzer::analyzeConstructor(ConstructorDecl& ctor) {
    MethodContext ctx;
    ctx.isStatic = false;
    ctx.returnType = Type::primitive(Type::Kind::Void);
    ctx.nextSlot = 1;  // slot 0 is 'this'
    currentMethod_ = &ctx;

    scopes_.clear();
    pushScope();

    for (auto& p : ctor.params) {
        checkNoArray(p.type, ctor.line, ctor.col, "parameter");
        LocalInfo* info = declareLocal(p.name, p.type, ctor.line, ctor.col);
        if (info) p.slot = info->slot;
    }

    if (ctor.body) {
        for (auto& s : ctor.body->stmts) analyzeStmt(*s);
    }

    ctor.maxLocals = ctx.nextSlot;
    popScope();
    currentMethod_ = nullptr;
}

void SemanticAnalyzer::analyzeBlock(BlockStmt& block) {
    pushScope();
    for (auto& s : block.stmts) analyzeStmt(*s);
    popScope();
}

void SemanticAnalyzer::analyzeStmt(Stmt& stmt) {
    switch (stmt.kind) {
        case StmtKind::Block:
            analyzeBlock(static_cast<BlockStmt&>(stmt));
            return;

        case StmtKind::LocalVarDecl: {
            auto& d = static_cast<LocalVarDeclStmt&>(stmt);
            checkNoArray(d.type, d.line, d.col, "local variable");
            if (d.init) {
                analyzeExpr(*d.init);
                if (d.init->typeResolved && !assignable(d.type, d.init->resolvedType)) {
                    error(d.init->line, d.init->col,
                          "cannot initialize '" + d.name + "' of type " + toString(d.type) +
                              " with a value of type " + toString(d.init->resolvedType));
                }
            }
            LocalInfo* info = declareLocal(d.name, d.type, d.line, d.col);
            if (info) d.slot = info->slot;
            return;
        }

        case StmtKind::If: {
            auto& i = static_cast<IfStmt&>(stmt);
            analyzeExpr(*i.cond);
            requireBoolean(*i.cond, "if condition");
            analyzeStmt(*i.thenBranch);
            if (i.elseBranch) analyzeStmt(*i.elseBranch);
            return;
        }

        case StmtKind::While: {
            auto& w = static_cast<WhileStmt&>(stmt);
            analyzeExpr(*w.cond);
            requireBoolean(*w.cond, "while condition");
            analyzeStmt(*w.body);
            return;
        }

        case StmtKind::For: {
            auto& f = static_cast<ForStmt&>(stmt);
            pushScope();
            if (f.init) analyzeStmt(*f.init);
            if (f.cond) {
                analyzeExpr(*f.cond);
                requireBoolean(*f.cond, "for condition");
            }
            if (f.update) analyzeExpr(*f.update);
            analyzeStmt(*f.body);
            popScope();
            return;
        }

        case StmtKind::Return: {
            auto& r = static_cast<ReturnStmt&>(stmt);
            bool isVoid = currentMethod_->returnType.kind == Type::Kind::Void;
            if (isVoid) {
                if (r.value) {
                    error(r.line, r.col, "unexpected return value in a void method/constructor");
                    analyzeExpr(*r.value);
                }
            } else if (!r.value) {
                error(r.line, r.col, "missing return value");
            } else {
                analyzeExpr(*r.value);
                if (r.value->typeResolved &&
                    !assignable(currentMethod_->returnType, r.value->resolvedType)) {
                    error(r.value->line, r.value->col,
                          "cannot return a value of type " + toString(r.value->resolvedType) +
                              " from a method returning " + toString(currentMethod_->returnType));
                }
            }
            return;
        }

        case StmtKind::ExprStmt: {
            auto& e = static_cast<ExprStmt&>(stmt);
            analyzeExpr(*e.expr);
            return;
        }
    }
}

void SemanticAnalyzer::analyzeExpr(Expr& expr) {
    switch (expr.kind) {
        case ExprKind::IntLiteral:
            expr.resolvedType = Type::primitive(Type::Kind::Int);
            expr.typeResolved = true;
            return;

        case ExprKind::DoubleLiteral:
            expr.resolvedType = Type::primitive(Type::Kind::Double);
            expr.typeResolved = true;
            return;

        case ExprKind::BoolLiteral:
            expr.resolvedType = Type::primitive(Type::Kind::Boolean);
            expr.typeResolved = true;
            return;

        case ExprKind::StringLiteral:
            expr.resolvedType = Type::classRef("String");
            expr.typeResolved = true;
            return;

        case ExprKind::This: {
            if (currentMethod_->isStatic) {
                error(expr.line, expr.col, "'this' cannot be used in a static context");
                return;
            }
            expr.resolvedType = Type::classRef(classTable_.className());
            expr.typeResolved = true;
            return;
        }

        case ExprKind::Name: {
            auto& n = static_cast<NameExpr&>(expr);
            if (LocalInfo* local = lookupLocal(n.name)) {
                n.refKind = NameExpr::RefKind::Local;
                n.slot = local->slot;
                n.resolvedType = local->type;
                n.typeResolved = true;
                return;
            }
            if (const FieldSymbol* f = classTable_.findField(n.name)) {
                if (currentMethod_->isStatic) {
                    error(n.line, n.col,
                          "cannot reference instance field '" + n.name +
                              "' from a static context");
                    return;
                }
                n.refKind = NameExpr::RefKind::Field;
                n.resolvedType = f->type;
                n.typeResolved = true;
                return;
            }
            error(n.line, n.col, "unknown identifier '" + n.name + "'");
            return;
        }

        case ExprKind::FieldAccess:
            analyzeFieldAccess(static_cast<FieldAccessExpr&>(expr));
            return;

        case ExprKind::MethodCall:
            analyzeMethodCall(static_cast<MethodCallExpr&>(expr));
            return;

        case ExprKind::New: {
            auto& n = static_cast<NewExpr&>(expr);
            if (n.className != classTable_.className()) {
                error(n.line, n.col,
                      "unknown class '" + n.className +
                          "' (v0 only supports the class declared in this file)");
                for (auto& a : n.args) analyzeExpr(*a);
                return;
            }
            static const std::vector<Type> kEmptyParams;
            const std::vector<Type>& ctorParams =
                classTable_.hasExplicitConstructor() ? classTable_.constructorParamTypes() : kEmptyParams;
            if (n.args.size() != ctorParams.size()) {
                error(n.line, n.col,
                      "constructor for '" + n.className + "' expects " +
                          std::to_string(ctorParams.size()) + " argument(s) but got " +
                          std::to_string(n.args.size()));
            }
            for (size_t i = 0; i < n.args.size(); i++) {
                analyzeExpr(*n.args[i]);
                if (i < ctorParams.size() && n.args[i]->typeResolved &&
                    !assignable(ctorParams[i], n.args[i]->resolvedType)) {
                    error(n.args[i]->line, n.args[i]->col,
                          "argument " + std::to_string(i + 1) + " to constructor: expected " +
                              toString(ctorParams[i]) + " but got " +
                              toString(n.args[i]->resolvedType));
                }
            }
            n.resolvedType = Type::classRef(n.className);
            n.typeResolved = true;
            return;
        }

        case ExprKind::Assign: {
            auto& a = static_cast<AssignExpr&>(expr);
            analyzeExpr(*a.target);
            analyzeExpr(*a.value);
            if (a.target->typeResolved && a.value->typeResolved) {
                if (!assignable(a.target->resolvedType, a.value->resolvedType)) {
                    error(a.value->line, a.value->col,
                          "cannot assign a value of type " + toString(a.value->resolvedType) +
                              " to a target of type " + toString(a.target->resolvedType));
                } else {
                    a.resolvedType = a.target->resolvedType;
                    a.typeResolved = true;
                }
            }
            return;
        }

        case ExprKind::Unary: {
            auto& u = static_cast<UnaryExpr&>(expr);
            analyzeExpr(*u.operand);
            if (!u.operand->typeResolved) return;
            if (u.op == UnaryOp::Neg) {
                if (!isNumeric(u.operand->resolvedType)) {
                    error(u.line, u.col, "unary '-' requires a numeric operand");
                    return;
                }
                u.resolvedType = u.operand->resolvedType;
                u.typeResolved = true;
            } else {
                if (u.operand->resolvedType.kind != Type::Kind::Boolean) {
                    error(u.line, u.col, "unary '!' requires a boolean operand");
                    return;
                }
                u.resolvedType = Type::primitive(Type::Kind::Boolean);
                u.typeResolved = true;
            }
            return;
        }

        case ExprKind::Binary: {
            auto& b = static_cast<BinaryExpr&>(expr);
            analyzeExpr(*b.left);
            analyzeExpr(*b.right);
            if (!b.left->typeResolved || !b.right->typeResolved) return;
            const Type& lt = b.left->resolvedType;
            const Type& rt = b.right->resolvedType;

            switch (b.op) {
                case BinaryOp::Add:
                case BinaryOp::Sub:
                case BinaryOp::Mul:
                case BinaryOp::Div:
                case BinaryOp::Mod: {
                    if (b.op == BinaryOp::Add && (lt.kind == Type::Kind::ClassRef ||
                                                   rt.kind == Type::Kind::ClassRef)) {
                        error(b.line, b.col, "string concatenation with '+' is not supported yet");
                        return;
                    }
                    if (!isNumeric(lt) || !isNumeric(rt)) {
                        error(b.line, b.col, "arithmetic operator requires numeric operands");
                        return;
                    }
                    b.resolvedType = (lt.kind == Type::Kind::Double || rt.kind == Type::Kind::Double)
                                         ? Type::primitive(Type::Kind::Double)
                                         : Type::primitive(Type::Kind::Int);
                    b.typeResolved = true;
                    return;
                }
                case BinaryOp::Lt:
                case BinaryOp::LtEq:
                case BinaryOp::Gt:
                case BinaryOp::GtEq: {
                    if (!isNumeric(lt) || !isNumeric(rt)) {
                        error(b.line, b.col, "relational operator requires numeric operands");
                        return;
                    }
                    b.resolvedType = Type::primitive(Type::Kind::Boolean);
                    b.typeResolved = true;
                    return;
                }
                case BinaryOp::Eq:
                case BinaryOp::NotEq: {
                    bool ok = (isNumeric(lt) && isNumeric(rt)) ||
                              (lt.kind == Type::Kind::Boolean && rt.kind == Type::Kind::Boolean) ||
                              (lt.kind == Type::Kind::ClassRef && rt.kind == Type::Kind::ClassRef &&
                               lt.className == rt.className);
                    if (!ok) {
                        error(b.line, b.col,
                              "cannot compare " + toString(lt) + " and " + toString(rt));
                        return;
                    }
                    b.resolvedType = Type::primitive(Type::Kind::Boolean);
                    b.typeResolved = true;
                    return;
                }
                case BinaryOp::And:
                case BinaryOp::Or: {
                    if (lt.kind != Type::Kind::Boolean || rt.kind != Type::Kind::Boolean) {
                        error(b.line, b.col, "logical operator requires boolean operands");
                        return;
                    }
                    b.resolvedType = Type::primitive(Type::Kind::Boolean);
                    b.typeResolved = true;
                    return;
                }
            }
            return;
        }

        case ExprKind::Error:
            return;
    }
}

void SemanticAnalyzer::analyzeMethodCall(MethodCallExpr& call) {
    if (isPrintlnPattern(call)) {
        call.callKind = MethodCallExpr::CallKind::PrintlnSpecial;
        analyzeExpr(*call.args[0]);
        if (call.args[0]->typeResolved) {
            const Type& t = call.args[0]->resolvedType;
            bool ok = t.kind == Type::Kind::Int || t.kind == Type::Kind::Double ||
                      t.kind == Type::Kind::Boolean ||
                      (t.kind == Type::Kind::ClassRef && t.className == "String");
            if (!ok) {
                error(call.line, call.col,
                      "System.out.println does not support argument type " + toString(t) +
                          " yet");
                return;
            }
        }
        call.resolvedType = Type::primitive(Type::Kind::Void);
        call.typeResolved = true;
        return;
    }

    const MethodSymbol* method = classTable_.findMethod(call.name);
    if (!method) {
        error(call.line, call.col, "unknown method '" + call.name + "'");
        if (call.target) analyzeExpr(*call.target);
        for (auto& a : call.args) analyzeExpr(*a);
        return;
    }

    bool targetOk = true;
    if (call.target) {
        analyzeExpr(*call.target);
        if (!call.target->typeResolved) {
            targetOk = false;
        } else if (call.target->resolvedType.kind != Type::Kind::ClassRef ||
                   call.target->resolvedType.className != classTable_.className()) {
            error(call.target->line, call.target->col,
                  "cannot call a method on a value of type " +
                      toString(call.target->resolvedType));
            targetOk = false;
        }
    } else if (currentMethod_->isStatic && !method->isStatic) {
        error(call.line, call.col,
              "cannot call instance method '" + call.name + "' from a static context");
        targetOk = false;
    }
    if (call.args.size() != method->paramTypes.size()) {
        error(call.line, call.col,
              "method '" + call.name + "' expects " + std::to_string(method->paramTypes.size()) +
                  " argument(s) but got " + std::to_string(call.args.size()));
    }
    for (size_t i = 0; i < call.args.size(); i++) {
        analyzeExpr(*call.args[i]);
        if (i < method->paramTypes.size() && call.args[i]->typeResolved &&
            !assignable(method->paramTypes[i], call.args[i]->resolvedType)) {
            error(call.args[i]->line, call.args[i]->col,
                  "argument " + std::to_string(i + 1) + " to '" + call.name +
                      "': expected " + toString(method->paramTypes[i]) + " but got " +
                      toString(call.args[i]->resolvedType));
        }
    }

    if (!targetOk) return;
    call.callKind = MethodCallExpr::CallKind::UserMethod;
    call.resolvedType = method->returnType;
    call.typeResolved = true;
}

void SemanticAnalyzer::analyzeFieldAccess(FieldAccessExpr& fa) {
    analyzeExpr(*fa.target);
    if (!fa.target->typeResolved) return;
    if (fa.target->resolvedType.kind != Type::Kind::ClassRef ||
        fa.target->resolvedType.className != classTable_.className()) {
        error(fa.line, fa.col,
              "cannot access member '" + fa.name + "' on a value of type " +
                  toString(fa.target->resolvedType));
        return;
    }
    const FieldSymbol* f = classTable_.findField(fa.name);
    if (!f) {
        error(fa.line, fa.col, "unknown field '" + fa.name + "'");
        return;
    }
    fa.resolvedType = f->type;
    fa.typeResolved = true;
}

}  // namespace jc
