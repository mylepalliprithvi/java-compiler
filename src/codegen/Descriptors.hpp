#pragma once

#include <string>
#include <vector>

#include "ast/Type.hpp"

namespace jc {

// Maps a v0 simple class name to its JVM binary name. `ownClassName` is the
// class being compiled (assumed default package, so it maps to itself); a
// small fixed table covers the external names v0 ever references (String,
// Object). Anything else is assumed to already be a default-package name —
// see the "extends" limitation noted in CLAUDE.md.
std::string binaryClassName(const std::string& simpleName, const std::string& ownClassName);

// JVM field descriptor for a type, e.g. Int -> "I", ClassRef("String") ->
// "Ljava/lang/String;".
std::string typeDescriptor(const Type& type, const std::string& ownClassName);

// JVM method descriptor, e.g. "(ID)Ljava/lang/String;".
std::string methodDescriptor(const std::vector<Type>& paramTypes, const Type& returnType,
                              const std::string& ownClassName);

}  // namespace jc
