#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ast/Stmt.hpp"
#include "ast/Type.hpp"

namespace jc {

struct Param {
    Type type;
    std::string name;
};

struct FieldDecl {
    Type type;
    std::string name;
    bool isPublic = true;
    int line = 0;
    int col = 0;
};

struct ConstructorDecl {
    bool isPublic = true;
    std::vector<Param> params;
    std::unique_ptr<BlockStmt> body;
    int line = 0;
    int col = 0;
};

struct MethodDecl {
    bool isPublic = true;
    bool isStatic = false;
    Type returnType;
    std::string name;
    std::vector<Param> params;
    std::unique_ptr<BlockStmt> body;
    int line = 0;
    int col = 0;
};

struct ClassDecl {
    bool isPublic = true;
    std::string name;
    std::optional<std::string> superclass;
    std::vector<FieldDecl> fields;
    std::vector<ConstructorDecl> constructors;
    std::vector<MethodDecl> methods;
    int line = 0;
    int col = 0;
};

struct CompilationUnit {
    ClassDecl classDecl;
};

}  // namespace jc
