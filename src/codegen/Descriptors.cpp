#include "codegen/Descriptors.hpp"

#include <unordered_map>

namespace jc {

std::string binaryClassName(const std::string& simpleName, const std::string& ownClassName) {
    if (simpleName == ownClassName) return simpleName;
    static const std::unordered_map<std::string, std::string> kKnown = {
        {"String", "java/lang/String"},
        {"Object", "java/lang/Object"},
    };
    auto it = kKnown.find(simpleName);
    if (it != kKnown.end()) return it->second;
    return simpleName;  // best effort: assume default package (see CLAUDE.md)
}

std::string typeDescriptor(const Type& type, const std::string& ownClassName) {
    switch (type.kind) {
        case Type::Kind::Int: return "I";
        case Type::Kind::Boolean: return "Z";
        case Type::Kind::Double: return "D";
        case Type::Kind::Void: return "V";
        case Type::Kind::ClassRef:
            return "L" + binaryClassName(type.className, ownClassName) + ";";
        case Type::Kind::ArrayOfClassRef:
            return "[L" + binaryClassName(type.className, ownClassName) + ";";
    }
    return "V";
}

std::string methodDescriptor(const std::vector<Type>& paramTypes, const Type& returnType,
                              const std::string& ownClassName) {
    std::string desc = "(";
    for (const auto& p : paramTypes) desc += typeDescriptor(p, ownClassName);
    desc += ")";
    desc += typeDescriptor(returnType, ownClassName);
    return desc;
}

}  // namespace jc
