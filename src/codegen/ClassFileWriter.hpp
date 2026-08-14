#pragma once

#include <cstdint>
#include <vector>

#include "ast/Decl.hpp"
#include "sema/ClassTable.hpp"

namespace jc {

// Assembles a full JVM .class file (major version 49 — see docs/subset-v0.md
// for why) for a sema-clean CompilationUnit. Caller must have already run
// SemanticAnalyzer::analyze() successfully; this does no error checking of
// its own and trusts every resolved field on the AST.
std::vector<uint8_t> generateClassFile(const CompilationUnit& unit, const ClassTable& classTable);

}  // namespace jc
