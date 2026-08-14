
#include "sema/ClassTable.hpp"

namespace jc {

void ClassTable::build(const ClassDecl& classDecl,
                        const std::function<void(int, int, std::string)>& onError) {
    className_ = classDecl.name;
    superclass_ = classDecl.superclass;

    for (const auto& f : classDecl.fields) {
        if (fields_.count(f.name)) {
            onError(f.line, f.col, "duplicate field '" + f.name + "'");
            continue;
        }
        fields_[f.name] = FieldSymbol{f.type, f.name, f.isPublic};
    }

    for (const auto& m : classDecl.methods) {
        if (methods_.count(m.name)) {
            onError(m.line, m.col, "duplicate method '" + m.name + "' (overloading is not supported yet)");
            continue;
        }
        MethodSymbol sym;
        sym.isStatic = m.isStatic;
        sym.returnType = m.returnType;
        sym.name = m.name;
        for (const auto& p : m.params) sym.paramTypes.push_back(p.type);
        sym.decl = &m;
        methods_[m.name] = std::move(sym);
    }

    if (!classDecl.constructors.empty()) {
        hasCtor_ = true;
        const auto& ctor = classDecl.constructors.front();
        for (const auto& p : ctor.params) ctorParamTypes_.push_back(p.type);
        if (classDecl.constructors.size() > 1) {
            onError(classDecl.constructors[1].line, classDecl.constructors[1].col,
                    "multiple constructors are not supported yet");
        }
    }
}

const FieldSymbol* ClassTable::findField(const std::string& name) const {
    auto it = fields_.find(name);
    return it == fields_.end() ? nullptr : &it->second;
}

const MethodSymbol* ClassTable::findMethod(const std::string& name) const {
    auto it = methods_.find(name);
    return it == methods_.end() ? nullptr : &it->second;
}

}  // namespace jc
