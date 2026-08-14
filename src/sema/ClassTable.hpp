#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast/Decl.hpp"
#include "ast/Type.hpp"

namespace jc {

struct FieldSymbol {
    Type type;
    std::string name;
    bool isPublic = true;
};

struct MethodSymbol {
    bool isStatic = false;
    Type returnType;
    std::string name;
    std::vector<Type> paramTypes;
    const MethodDecl* decl = nullptr;
};

// v0 has exactly one user-declared class per compilation unit (see
// docs/subset-v0.md), so this is really "the symbol table for that class" —
// not a multi-class registry. `extends` is recorded for codegen (super_class
// / super() calls) but member lookup does NOT walk into the superclass:
// v0 has no multi-file compilation, so nothing is known about it beyond its
// name (see CLAUDE.md).
class ClassTable {
public:
    // Populates the table from a class declaration and records duplicate
    // field/method names as diagnostics via `onError`. No overloading in v0:
    // a method name is a single symbol.
    void build(const ClassDecl& classDecl, const std::function<void(int, int, std::string)>& onError);

    const std::string& className() const { return className_; }
    const std::optional<std::string>& superclass() const { return superclass_; }

    const FieldSymbol* findField(const std::string& name) const;
    const MethodSymbol* findMethod(const std::string& name) const;

    // Constructor parameter types; empty if the class relies on the implicit
    // no-arg constructor.
    bool hasExplicitConstructor() const { return hasCtor_; }
    const std::vector<Type>& constructorParamTypes() const { return ctorParamTypes_; }

private:
    std::string className_;
    std::optional<std::string> superclass_;
    std::unordered_map<std::string, FieldSymbol> fields_;
    std::unordered_map<std::string, MethodSymbol> methods_;
    bool hasCtor_ = false;
    std::vector<Type> ctorParamTypes_;
};

}  // namespace jc
