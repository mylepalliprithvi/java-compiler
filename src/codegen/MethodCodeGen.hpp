#pragma once

#include <cstddef>
#include <string>

#include "ast/Decl.hpp"
#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "ast/Type.hpp"
#include "codegen/ByteWriter.hpp"
#include "codegen/ConstantPool.hpp"
#include "sema/ClassTable.hpp"

namespace jc {

// Emits the bytecode for a single method or constructor body into a
// ByteWriter, tracking operand-stack depth as it goes (for max_stack).
// Trusts the AST's sema-attributed fields (resolvedType, refKind, slot,
// callKind) completely — this stage does no further type checking.
class MethodCodeGen {
public:
    MethodCodeGen(ConstantPool& pool, const ClassTable& classTable, const std::string& className,
                  const std::string& superBinaryName, bool isStatic, Type returnType);

    // Emits an implicit `super()` call (aload_0; invokespecial <super>.<init>()V)
    // — every v0 constructor starts with this; see CLAUDE.md.
    void emitImplicitSuperCall();

    // Appended unconditionally after a void method/constructor body: cheap
    // and always correct (a no-op if the body already returns on every
    // reachable path; otherwise the only thing preventing the bytecode from
    // illegally falling off the end of the Code array). Non-void methods
    // get no such safety net — see CLAUDE.md's "missing return" note.
    void emitVoidReturn();

    void genStatements(const std::vector<StmtPtr>& stmts);

    const ByteWriter& code() const { return code_; }
    int maxStack() const { return maxStack_; }

private:
    void push(int slots);
    void pop(int slots);

    void genStmt(Stmt& stmt);
    void genExpr(Expr& expr);
    // Evaluates expr for its side effects and discards any leftover value
    // (matching JVM's requirement of zero net stack effect between
    // statements) — used for ExprStmt and for-loop update clauses. Skips
    // the discard for Assign, which never leaves a value (see genAssign).
    void genExprAsStatement(Expr& expr);
    void genMethodCall(MethodCallExpr& call);
    void genAssign(AssignExpr& assign);
    void genBinary(BinaryExpr& bin);
    void genShortCircuit(BinaryExpr& bin);
    void genComparison(BinaryExpr& bin, const Type& operandType);

    void genIntConst(long long value);
    void genLoad(const Type& type, int slot);
    void genStore(const Type& type, int slot);
    void maybePromoteToDouble(Expr& operand, bool needDouble);

    size_t emitBranch(uint8_t opcode);      // returns position of the opcode byte
    void patchBranchHere(size_t opcodePos);  // target = current position
    void patchBranchTo(size_t opcodePos, size_t targetPos);

    static int slotSize(const Type& type);
    static bool isNumeric(const Type& type);
    // True if every control path through stmt ends in a return. Used to
    // skip emitting a dead "goto past-the-end" after an if/else whose
    // then-branch always returns — that goto's target would otherwise be
    // one byte past the last instruction in the method, which the JVM
    // verifier rejects ("Illegal target of jump or branch"). Deliberately
    // conservative (While/For are never assumed to always return).
    static bool alwaysReturns(const Stmt& stmt);
    std::string descriptor(const Type& type) const;
    std::string binaryName(const std::string& simpleClassName) const;

    ConstantPool& pool_;
    const ClassTable& classTable_;
    std::string className_;
    std::string superBinaryName_;
    bool isStatic_;
    Type returnType_;

    ByteWriter code_;
    int stackDepth_ = 0;
    int maxStack_ = 0;
};

}  // namespace jc
