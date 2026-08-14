#pragma once

#include <string>

namespace jc {

// v0 type representation. ArrayOfClassRef exists only so method signatures
// can spell `String[] args` (needed for a real JVM `main` entry point) —
// arrays are not otherwise supported yet (see docs/subset-v0.md).
struct Type {
    enum class Kind { Int, Boolean, Double, Void, ClassRef, ArrayOfClassRef };

    Kind kind = Kind::Void;
    std::string className;  // set when kind is ClassRef or ArrayOfClassRef

    static Type primitive(Kind k) { return Type{k, ""}; }
    static Type classRef(std::string name) { return Type{Kind::ClassRef, std::move(name)}; }
    static Type arrayOfClassRef(std::string name) {
        return Type{Kind::ArrayOfClassRef, std::move(name)};
    }

    bool operator==(const Type& other) const {
        return kind == other.kind && className == other.className;
    }
};

std::string toString(const Type& type);

}  // namespace jc
