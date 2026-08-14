#pragma once

#include <string>

#include "ast/Decl.hpp"

namespace jc {

// Deterministic S-expression rendering of an AST, used by parser tests to
// assert tree shape without hand-building expected trees.
std::string printAst(const CompilationUnit& unit);

}  // namespace jc
