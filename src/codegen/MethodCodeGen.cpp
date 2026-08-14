#include "codegen/MethodCodeGen.hpp"

#include "codegen/Descriptors.hpp"
#include "codegen/Opcodes.hpp"

namespace jc {

MethodCodeGen::MethodCodeGen(ConstantPool& pool, const ClassTable& classTable,
                              const std::string& className, const std::string& superBinaryName,
                              bool isStatic, Type returnType)
    : pool_(pool),
      classTable_(classTable),
      className_(className),
      superBinaryName_(superBinaryName),
      isStatic_(isStatic),
      returnType_(std::move(returnType)) {}

int MethodCodeGen::slotSize(const Type& type) { return type.kind == Type::Kind::Double ? 2 : 1; }

bool MethodCodeGen::isNumeric(const Type& type) {
    return type.kind == Type::Kind::Int || type.kind == Type::Kind::Double;
}

bool MethodCodeGen::alwaysReturns(const Stmt& stmt) {
    switch (stmt.kind) {
        case StmtKind::Return:
            return true;
        case StmtKind::Block: {
            const auto& b = static_cast<const BlockStmt&>(stmt);
            if (b.stmts.empty()) return false;
            return alwaysReturns(*b.stmts.back());
        }
        case StmtKind::If: {
            const auto& i = static_cast<const IfStmt&>(stmt);
            if (!i.elseBranch) return false;
            return alwaysReturns(*i.thenBranch) && alwaysReturns(*i.elseBranch);
        }
        default:
            return false;  // While/For/LocalVarDecl/ExprStmt
    }
}

std::string MethodCodeGen::descriptor(const Type& type) const {
    return typeDescriptor(type, className_);
}

std::string MethodCodeGen::binaryName(const std::string& simpleClassName) const {
    return binaryClassName(simpleClassName, className_);
}

void MethodCodeGen::push(int slots) {
    stackDepth_ += slots;
    if (stackDepth_ > maxStack_) maxStack_ = stackDepth_;
}

void MethodCodeGen::pop(int slots) { stackDepth_ -= slots; }

size_t MethodCodeGen::emitBranch(uint8_t opcode) {
    size_t pos = code_.size();
    code_.writeU1(opcode);
    code_.writeU2(0);  // placeholder, patched later
    return pos;
}

void MethodCodeGen::patchBranchTo(size_t opcodePos, size_t targetPos) {
    int32_t offset = static_cast<int32_t>(targetPos) - static_cast<int32_t>(opcodePos);
    code_.patchU2(opcodePos + 1, static_cast<uint16_t>(static_cast<int16_t>(offset)));
}

void MethodCodeGen::patchBranchHere(size_t opcodePos) { patchBranchTo(opcodePos, code_.size()); }

void MethodCodeGen::emitImplicitSuperCall() {
    code_.writeU1(op::aload_0);
    push(1);
    uint16_t methodIdx = pool_.addMethodref(superBinaryName_, "<init>", "()V");
    code_.writeU1(op::invokespecial);
    code_.writeU2(methodIdx);
    pop(1);
}

void MethodCodeGen::emitVoidReturn() { code_.writeU1(op::return_); }

void MethodCodeGen::genStatements(const std::vector<StmtPtr>& stmts) {
    for (const auto& s : stmts) genStmt(*s);
}

void MethodCodeGen::genIntConst(long long value) {
    if (value >= -1 && value <= 5) {
        code_.writeU1(static_cast<uint8_t>(op::iconst_0 + value));
        push(1);
        return;
    }
    if (value >= -128 && value <= 127) {
        code_.writeU1(op::bipush);
        code_.writeU1(static_cast<uint8_t>(value));
        push(1);
        return;
    }
    if (value >= -32768 && value <= 32767) {
        code_.writeU1(op::sipush);
        code_.writeU2(static_cast<uint16_t>(value));
        push(1);
        return;
    }
    uint16_t idx = pool_.addInteger(static_cast<int32_t>(value));
    if (idx <= 0xff) {
        code_.writeU1(op::ldc);
        code_.writeU1(static_cast<uint8_t>(idx));
    } else {
        code_.writeU1(op::ldc_w);
        code_.writeU2(idx);
    }
    push(1);
}

void MethodCodeGen::genLoad(const Type& type, int slot) {
    if (type.kind == Type::Kind::Double) {
        if (slot <= 3) {
            code_.writeU1(static_cast<uint8_t>(op::dload_0 + slot));
        } else {
            code_.writeU1(op::dload);
            code_.writeU1(static_cast<uint8_t>(slot));
        }
        push(2);
    } else if (type.kind == Type::Kind::ClassRef || type.kind == Type::Kind::ArrayOfClassRef) {
        if (slot <= 3) {
            code_.writeU1(static_cast<uint8_t>(op::aload_0 + slot));
        } else {
            code_.writeU1(op::aload);
            code_.writeU1(static_cast<uint8_t>(slot));
        }
        push(1);
    } else {
        if (slot <= 3) {
            code_.writeU1(static_cast<uint8_t>(op::iload_0 + slot));
        } else {
            code_.writeU1(op::iload);
            code_.writeU1(static_cast<uint8_t>(slot));
        }
        push(1);
    }
}

void MethodCodeGen::genStore(const Type& type, int slot) {
    if (type.kind == Type::Kind::Double) {
        if (slot <= 3) {
            code_.writeU1(static_cast<uint8_t>(op::dstore_0 + slot));
        } else {
            code_.writeU1(op::dstore);
            code_.writeU1(static_cast<uint8_t>(slot));
        }
    } else if (type.kind == Type::Kind::ClassRef || type.kind == Type::Kind::ArrayOfClassRef) {
        if (slot <= 3) {
            code_.writeU1(static_cast<uint8_t>(op::astore_0 + slot));
        } else {
            code_.writeU1(op::astore);
            code_.writeU1(static_cast<uint8_t>(slot));
        }
    } else {
        if (slot <= 3) {
            code_.writeU1(static_cast<uint8_t>(op::istore_0 + slot));
        } else {
            code_.writeU1(op::istore);
            code_.writeU1(static_cast<uint8_t>(slot));
        }
    }
}

void MethodCodeGen::maybePromoteToDouble(Expr& operand, bool needDouble) {
    if (needDouble && operand.resolvedType.kind == Type::Kind::Int) {
        code_.writeU1(op::i2d);
        pop(1);
        push(2);
    }
}

// --- statements ---

void MethodCodeGen::genStmt(Stmt& stmt) {
    switch (stmt.kind) {
        case StmtKind::Block: {
            genStatements(static_cast<BlockStmt&>(stmt).stmts);
            return;
        }
        case StmtKind::LocalVarDecl: {
            auto& d = static_cast<LocalVarDeclStmt&>(stmt);
            if (d.init) {
                genExpr(*d.init);
                genStore(d.type, d.slot);
                pop(slotSize(d.type));
            }
            return;
        }
        case StmtKind::If: {
            auto& i = static_cast<IfStmt&>(stmt);
            genExpr(*i.cond);
            size_t jFalse = emitBranch(op::ifeq);
            pop(1);
            genStmt(*i.thenBranch);
            if (i.elseBranch) {
                bool thenReturns = alwaysReturns(*i.thenBranch);
                size_t jEnd = 0;
                if (!thenReturns) jEnd = emitBranch(op::goto_);
                patchBranchHere(jFalse);
                genStmt(*i.elseBranch);
                if (!thenReturns) patchBranchHere(jEnd);
            } else {
                patchBranchHere(jFalse);
            }
            return;
        }
        case StmtKind::While: {
            auto& w = static_cast<WhileStmt&>(stmt);
            size_t loopStart = code_.size();
            genExpr(*w.cond);
            size_t jExit = emitBranch(op::ifeq);
            pop(1);
            genStmt(*w.body);
            size_t g = emitBranch(op::goto_);
            patchBranchTo(g, loopStart);
            patchBranchHere(jExit);
            return;
        }
        case StmtKind::For: {
            auto& f = static_cast<ForStmt&>(stmt);
            if (f.init) genStmt(*f.init);
            size_t loopStart = code_.size();
            size_t jExit = 0;
            bool hasExit = false;
            if (f.cond) {
                genExpr(*f.cond);
                jExit = emitBranch(op::ifeq);
                pop(1);
                hasExit = true;
            }
            genStmt(*f.body);
            if (f.update) genExprAsStatement(*f.update);
            size_t g = emitBranch(op::goto_);
            patchBranchTo(g, loopStart);
            if (hasExit) patchBranchHere(jExit);
            return;
        }
        case StmtKind::Return: {
            auto& r = static_cast<ReturnStmt&>(stmt);
            if (!r.value) {
                code_.writeU1(op::return_);
                return;
            }
            genExpr(*r.value);
            if (returnType_.kind == Type::Kind::Double) {
                code_.writeU1(op::dreturn);
                pop(2);
            } else if (returnType_.kind == Type::Kind::ClassRef ||
                       returnType_.kind == Type::Kind::ArrayOfClassRef) {
                code_.writeU1(op::areturn);
                pop(1);
            } else {
                code_.writeU1(op::ireturn);
                pop(1);
            }
            return;
        }
        case StmtKind::ExprStmt: {
            genExprAsStatement(*static_cast<ExprStmt&>(stmt).expr);
            return;
        }
    }
}

void MethodCodeGen::genExprAsStatement(Expr& expr) {
    genExpr(expr);
    if (expr.kind != ExprKind::Assign && expr.typeResolved &&
        expr.resolvedType.kind != Type::Kind::Void) {
        int n = slotSize(expr.resolvedType);
        code_.writeU1(n == 2 ? op::pop2 : op::pop);
        pop(n);
    }
}

// --- expressions ---

void MethodCodeGen::genExpr(Expr& expr) {
    switch (expr.kind) {
        case ExprKind::IntLiteral:
            genIntConst(static_cast<IntLiteralExpr&>(expr).value);
            return;

        case ExprKind::DoubleLiteral: {
            double v = static_cast<DoubleLiteralExpr&>(expr).value;
            if (v == 0.0) {
                code_.writeU1(op::dconst_0);
                push(2);
            } else if (v == 1.0) {
                code_.writeU1(op::dconst_1);
                push(2);
            } else {
                uint16_t idx = pool_.addDouble(v);
                code_.writeU1(op::ldc2_w);
                code_.writeU2(idx);
                push(2);
            }
            return;
        }

        case ExprKind::BoolLiteral: {
            bool v = static_cast<BoolLiteralExpr&>(expr).value;
            code_.writeU1(static_cast<uint8_t>(op::iconst_0 + (v ? 1 : 0)));
            push(1);
            return;
        }

        case ExprKind::StringLiteral: {
            uint16_t idx = pool_.addString(static_cast<StringLiteralExpr&>(expr).value);
            if (idx <= 0xff) {
                code_.writeU1(op::ldc);
                code_.writeU1(static_cast<uint8_t>(idx));
            } else {
                code_.writeU1(op::ldc_w);
                code_.writeU2(idx);
            }
            push(1);
            return;
        }

        case ExprKind::Name: {
            auto& n = static_cast<NameExpr&>(expr);
            if (n.refKind == NameExpr::RefKind::Local) {
                genLoad(n.resolvedType, n.slot);
            } else {
                code_.writeU1(op::aload_0);
                push(1);
                uint16_t idx = pool_.addFieldref(className_, n.name, descriptor(n.resolvedType));
                code_.writeU1(op::getfield);
                code_.writeU2(idx);
                pop(1);
                push(slotSize(n.resolvedType));
            }
            return;
        }

        case ExprKind::This:
            code_.writeU1(op::aload_0);
            push(1);
            return;

        case ExprKind::FieldAccess: {
            auto& fa = static_cast<FieldAccessExpr&>(expr);
            genExpr(*fa.target);
            uint16_t idx = pool_.addFieldref(className_, fa.name, descriptor(fa.resolvedType));
            code_.writeU1(op::getfield);
            code_.writeU2(idx);
            pop(1);
            push(slotSize(fa.resolvedType));
            return;
        }

        case ExprKind::MethodCall:
            genMethodCall(static_cast<MethodCallExpr&>(expr));
            return;

        case ExprKind::New: {
            auto& n = static_cast<NewExpr&>(expr);
            uint16_t classIdx = pool_.addClass(binaryName(n.className));
            code_.writeU1(op::new_);
            code_.writeU2(classIdx);
            push(1);
            code_.writeU1(op::dup);
            push(1);

            std::vector<Type> ctorParamTypes;
            if (classTable_.hasExplicitConstructor()) ctorParamTypes = classTable_.constructorParamTypes();

            int argSlots = 0;
            for (auto& a : n.args) {
                genExpr(*a);
                argSlots += slotSize(a->resolvedType);
            }
            std::string desc =
                methodDescriptor(ctorParamTypes, Type::primitive(Type::Kind::Void), className_);
            uint16_t methodIdx = pool_.addMethodref(binaryName(n.className), "<init>", desc);
            code_.writeU1(op::invokespecial);
            code_.writeU2(methodIdx);
            pop(1 + argSlots);
            return;
        }

        case ExprKind::Assign:
            genAssign(static_cast<AssignExpr&>(expr));
            return;

        case ExprKind::Unary: {
            auto& u = static_cast<UnaryExpr&>(expr);
            genExpr(*u.operand);
            if (u.op == UnaryOp::Neg) {
                code_.writeU1(u.resolvedType.kind == Type::Kind::Double ? op::dneg : op::ineg);
            } else {
                code_.writeU1(static_cast<uint8_t>(op::iconst_0 + 1));
                push(1);
                code_.writeU1(op::ixor);
                pop(2);
                push(1);
            }
            return;
        }

        case ExprKind::Binary:
            genBinary(static_cast<BinaryExpr&>(expr));
            return;

        case ExprKind::Error:
            return;  // unreachable in a sema-clean tree
    }
}

void MethodCodeGen::genAssign(AssignExpr& a) {
    // Deliberately leaves nothing on the stack — see header comment on
    // genExprAsStatement and CLAUDE.md's M5 notes. Nested "x = (y = 1)"
    // would produce invalid bytecode; v0 only ever emits assignment at
    // statement position or a for-loop update clause.
    if (a.target->kind == ExprKind::Name) {
        auto& nt = static_cast<NameExpr&>(*a.target);
        if (nt.refKind == NameExpr::RefKind::Local) {
            genExpr(*a.value);
            genStore(nt.resolvedType, nt.slot);
            pop(slotSize(nt.resolvedType));
        } else {
            code_.writeU1(op::aload_0);
            push(1);
            genExpr(*a.value);
            uint16_t idx = pool_.addFieldref(className_, nt.name, descriptor(nt.resolvedType));
            code_.writeU1(op::putfield);
            code_.writeU2(idx);
            pop(1 + slotSize(nt.resolvedType));
        }
        return;
    }

    auto& fa = static_cast<FieldAccessExpr&>(*a.target);
    genExpr(*fa.target);
    genExpr(*a.value);
    uint16_t idx = pool_.addFieldref(className_, fa.name, descriptor(fa.resolvedType));
    code_.writeU1(op::putfield);
    code_.writeU2(idx);
    pop(1 + slotSize(fa.resolvedType));
}

void MethodCodeGen::genMethodCall(MethodCallExpr& call) {
    if (call.callKind == MethodCallExpr::CallKind::PrintlnSpecial) {
        uint16_t sysOut = pool_.addFieldref("java/lang/System", "out", "Ljava/io/PrintStream;");
        code_.writeU1(op::getstatic);
        code_.writeU2(sysOut);
        push(1);

        Expr& arg = *call.args[0];
        genExpr(arg);
        std::string argDesc;
        switch (arg.resolvedType.kind) {
            case Type::Kind::Int: argDesc = "I"; break;
            case Type::Kind::Double: argDesc = "D"; break;
            case Type::Kind::Boolean: argDesc = "Z"; break;
            default: argDesc = "Ljava/lang/String;"; break;
        }
        uint16_t methodIdx =
            pool_.addMethodref("java/io/PrintStream", "println", "(" + argDesc + ")V");
        code_.writeU1(op::invokevirtual);
        code_.writeU2(methodIdx);
        pop(1 + slotSize(arg.resolvedType));
        return;
    }

    const MethodSymbol* method = classTable_.findMethod(call.name);

    if (method->isStatic) {
        if (call.target) {
            genExpr(*call.target);
            int slots = slotSize(call.target->resolvedType);
            code_.writeU1(slots == 2 ? op::pop2 : op::pop);
            pop(slots);
        }
    } else {
        if (call.target) {
            genExpr(*call.target);
        } else {
            code_.writeU1(op::aload_0);
            push(1);
        }
    }

    int argSlotsTotal = 0;
    for (auto& a : call.args) {
        genExpr(*a);
        argSlotsTotal += slotSize(a->resolvedType);
    }

    std::string desc = methodDescriptor(method->paramTypes, method->returnType, className_);
    uint16_t methodIdx = pool_.addMethodref(className_, call.name, desc);

    if (method->isStatic) {
        code_.writeU1(op::invokestatic);
        code_.writeU2(methodIdx);
        pop(argSlotsTotal);
    } else {
        code_.writeU1(op::invokevirtual);
        code_.writeU2(methodIdx);
        pop(1 + argSlotsTotal);
    }

    if (method->returnType.kind != Type::Kind::Void) {
        push(slotSize(method->returnType));
    }
}

// --- binary operators ---

void MethodCodeGen::genBinary(BinaryExpr& b) {
    if (b.op == BinaryOp::And || b.op == BinaryOp::Or) {
        genShortCircuit(b);
        return;
    }

    bool useDouble =
        b.left->resolvedType.kind == Type::Kind::Double || b.right->resolvedType.kind == Type::Kind::Double;

    switch (b.op) {
        case BinaryOp::Add:
        case BinaryOp::Sub:
        case BinaryOp::Mul:
        case BinaryOp::Div:
        case BinaryOp::Mod: {
            genExpr(*b.left);
            maybePromoteToDouble(*b.left, useDouble);
            genExpr(*b.right);
            maybePromoteToDouble(*b.right, useDouble);
            uint8_t opcode;
            switch (b.op) {
                case BinaryOp::Add: opcode = useDouble ? op::dadd : op::iadd; break;
                case BinaryOp::Sub: opcode = useDouble ? op::dsub : op::isub; break;
                case BinaryOp::Mul: opcode = useDouble ? op::dmul : op::imul; break;
                case BinaryOp::Div: opcode = useDouble ? op::ddiv : op::idiv; break;
                default: opcode = useDouble ? op::drem : op::irem; break;
            }
            code_.writeU1(opcode);
            int slots = useDouble ? 2 : 1;
            pop(2 * slots);
            push(slots);
            return;
        }
        case BinaryOp::Lt:
        case BinaryOp::LtEq:
        case BinaryOp::Gt:
        case BinaryOp::GtEq: {
            genExpr(*b.left);
            maybePromoteToDouble(*b.left, useDouble);
            genExpr(*b.right);
            maybePromoteToDouble(*b.right, useDouble);
            genComparison(b, useDouble ? Type::primitive(Type::Kind::Double)
                                        : Type::primitive(Type::Kind::Int));
            return;
        }
        case BinaryOp::Eq:
        case BinaryOp::NotEq: {
            const Type& lt = b.left->resolvedType;
            if (isNumeric(lt)) {
                genExpr(*b.left);
                maybePromoteToDouble(*b.left, useDouble);
                genExpr(*b.right);
                maybePromoteToDouble(*b.right, useDouble);
                genComparison(b, useDouble ? Type::primitive(Type::Kind::Double)
                                            : Type::primitive(Type::Kind::Int));
            } else if (lt.kind == Type::Kind::Boolean) {
                genExpr(*b.left);
                genExpr(*b.right);
                genComparison(b, Type::primitive(Type::Kind::Int));
            } else {
                genExpr(*b.left);
                genExpr(*b.right);
                genComparison(b, lt);
            }
            return;
        }
        default:
            return;  // And/Or handled above
    }
}

void MethodCodeGen::genComparison(BinaryExpr& b, const Type& operandType) {
    uint8_t branchOpcode;
    int consumed;

    if (operandType.kind == Type::Kind::Double) {
        code_.writeU1(op::dcmpg);
        pop(4);
        push(1);
        consumed = 1;
        switch (b.op) {
            case BinaryOp::Lt: branchOpcode = op::iflt; break;
            case BinaryOp::LtEq: branchOpcode = op::ifle; break;
            case BinaryOp::Gt: branchOpcode = op::ifgt; break;
            case BinaryOp::GtEq: branchOpcode = op::ifge; break;
            case BinaryOp::Eq: branchOpcode = op::ifeq; break;
            default: branchOpcode = op::ifne; break;
        }
    } else if (operandType.kind == Type::Kind::ClassRef) {
        consumed = 2;
        branchOpcode = (b.op == BinaryOp::Eq) ? op::if_acmpeq : op::if_acmpne;
    } else {
        consumed = 2;
        switch (b.op) {
            case BinaryOp::Lt: branchOpcode = op::if_icmplt; break;
            case BinaryOp::LtEq: branchOpcode = op::if_icmple; break;
            case BinaryOp::Gt: branchOpcode = op::if_icmpgt; break;
            case BinaryOp::GtEq: branchOpcode = op::if_icmpge; break;
            case BinaryOp::Eq: branchOpcode = op::if_icmpeq; break;
            default: branchOpcode = op::if_icmpne; break;
        }
    }

    size_t jTrue = emitBranch(branchOpcode);
    pop(consumed);
    int baseDepth = stackDepth_;

    code_.writeU1(op::iconst_0);
    push(1);
    size_t jEnd = emitBranch(op::goto_);

    patchBranchHere(jTrue);
    stackDepth_ = baseDepth;  // alternate (mutually exclusive) path, same starting depth
    code_.writeU1(static_cast<uint8_t>(op::iconst_0 + 1));
    push(1);

    patchBranchHere(jEnd);
}

void MethodCodeGen::genShortCircuit(BinaryExpr& b) {
    if (b.op == BinaryOp::And) {
        genExpr(*b.left);
        size_t jFalse = emitBranch(op::ifeq);
        pop(1);
        int baseDepth = stackDepth_;
        genExpr(*b.right);
        size_t jEnd = emitBranch(op::goto_);
        patchBranchHere(jFalse);
        stackDepth_ = baseDepth;
        code_.writeU1(op::iconst_0);
        push(1);
        patchBranchHere(jEnd);
    } else {
        genExpr(*b.left);
        size_t jTrue = emitBranch(op::ifne);
        pop(1);
        int baseDepth = stackDepth_;
        genExpr(*b.right);
        size_t jEnd = emitBranch(op::goto_);
        patchBranchHere(jTrue);
        stackDepth_ = baseDepth;
        code_.writeU1(static_cast<uint8_t>(op::iconst_0 + 1));
        push(1);
        patchBranchHere(jEnd);
    }
}

}  // namespace jc
