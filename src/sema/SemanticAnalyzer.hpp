#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "ast/Decl.hpp"
#include "sema/ClassTable.hpp"

namespace jc {

struct SemaError {
    int line;
    int col;
    std::string message;
};

// Name resolution + type checking for the v0 subset (docs/subset-v0.md).
// Attributes the AST in place (Expr::resolvedType, NameExpr::refKind/slot,
// MethodCallExpr::callKind, LocalVarDeclStmt::slot, Param::slot,
// MethodDecl/ConstructorDecl::maxLocals) rather than building a separate
// typed tree — codegen (M5) reads those fields directly.
class SemanticAnalyzer {
public:
    // Returns true if the class type-checks with no errors. Either way,
    // errors() holds every diagnostic found (this does not stop at the
    // first one).
    bool analyze(CompilationUnit& unit);

    const std::vector<SemaError>& errors() const { return errors_; }
    const ClassTable& classTable() const { return classTable_; }

private:
    struct LocalInfo {
        Type type;
        int slot;
    };
    using Scope = std::unordered_map<std::string, LocalInfo>;

    struct MethodContext {
        bool isStatic;
        Type returnType;
        int nextSlot;
    };

    void error(int line, int col, std::string message);
    static int slotSize(const Type& type);

    void analyzeMethod(MethodDecl& method);
    void analyzeConstructor(ConstructorDecl& ctor);
    void checkNoArray(const Type& type, int line, int col, const char* what);

    void pushScope();
    void popScope();
    LocalInfo* declareLocal(const std::string& name, Type type, int line, int col);
    LocalInfo* lookupLocal(const std::string& name);

    void analyzeBlock(BlockStmt& block);
    void analyzeStmt(Stmt& stmt);
    void analyzeExpr(Expr& expr);
    void analyzeMethodCall(MethodCallExpr& call);
    void analyzeFieldAccess(FieldAccessExpr& fa);
    void requireBoolean(const Expr& e, const char* context);

    bool isPrintlnPattern(const MethodCallExpr& call) const;
    static bool assignable(const Type& target, const Type& value);
    static bool isNumeric(const Type& t);

    ClassTable classTable_;
    std::vector<SemaError> errors_;
    std::vector<Scope> scopes_;
    MethodContext* currentMethod_ = nullptr;
};

}  // namespace jc
