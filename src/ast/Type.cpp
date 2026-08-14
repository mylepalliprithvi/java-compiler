#include "ast/Type.hpp"

namespace jc {

std::string toString(const Type& type) {
    switch (type.kind) {
        case Type::Kind::Int: return "int";
        case Type::Kind::Boolean: return "boolean";
        case Type::Kind::Double: return "double";
        case Type::Kind::Void: return "void";
        case Type::Kind::ClassRef: return type.className;
        case Type::Kind::ArrayOfClassRef: return type.className + "[]";
    }
    return "?";
}

}  // namespace jc
