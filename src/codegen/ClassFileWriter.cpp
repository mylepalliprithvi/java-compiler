#include "codegen/ClassFileWriter.hpp"

#include "codegen/ByteWriter.hpp"
#include "codegen/ConstantPool.hpp"
#include "codegen/Descriptors.hpp"
#include "codegen/MethodCodeGen.hpp"

namespace jc {

namespace {

constexpr uint16_t kAccPublic = 0x0001;
constexpr uint16_t kAccPrivate = 0x0002;
constexpr uint16_t kAccStatic = 0x0008;
constexpr uint16_t kAccSuper = 0x0020;

void writeMethodInfo(ByteWriter& methodsBuf, ConstantPool& pool, uint16_t codeAttrNameIdx,
                      uint16_t flags, const std::string& name, const std::string& descriptor,
                      const ByteWriter& code, int maxStack, int maxLocals) {
    uint16_t nameIdx = pool.addUtf8(name);
    uint16_t descIdx = pool.addUtf8(descriptor);

    methodsBuf.writeU2(flags);
    methodsBuf.writeU2(nameIdx);
    methodsBuf.writeU2(descIdx);
    methodsBuf.writeU2(1);  // attributes_count: just Code

    uint32_t attrLen = 2 + 2 + 4 + static_cast<uint32_t>(code.size()) + 2 + 2;
    methodsBuf.writeU2(codeAttrNameIdx);
    methodsBuf.writeU4(attrLen);
    methodsBuf.writeU2(static_cast<uint16_t>(maxStack));
    methodsBuf.writeU2(static_cast<uint16_t>(maxLocals));
    methodsBuf.writeU4(static_cast<uint32_t>(code.size()));
    methodsBuf.writeBytes(code);
    methodsBuf.writeU2(0);  // exception_table_length
    methodsBuf.writeU2(0);  // Code attribute's own attributes_count
}

std::vector<Type> paramTypesOf(const std::vector<Param>& params) {
    std::vector<Type> types;
    types.reserve(params.size());
    for (const auto& p : params) types.push_back(p.type);
    return types;
}

}  // namespace

std::vector<uint8_t> generateClassFile(const CompilationUnit& unit, const ClassTable& classTable) {
    const ClassDecl& c = unit.classDecl;
    ConstantPool pool;

    std::string className = c.name;
    std::string superBinary = binaryClassName(c.superclass.value_or("Object"), className);

    uint16_t thisClassIdx = pool.addClass(className);
    uint16_t superClassIdx = pool.addClass(superBinary);
    uint16_t codeAttrNameIdx = pool.addUtf8("Code");

    ByteWriter fieldsBuf;
    uint16_t fieldCount = 0;
    for (const auto& f : c.fields) {
        uint16_t nameIdx = pool.addUtf8(f.name);
        uint16_t descIdx = pool.addUtf8(typeDescriptor(f.type, className));
        fieldsBuf.writeU2(f.isPublic ? kAccPublic : kAccPrivate);
        fieldsBuf.writeU2(nameIdx);
        fieldsBuf.writeU2(descIdx);
        fieldsBuf.writeU2(0);  // attributes_count
        fieldCount++;
    }

    ByteWriter methodsBuf;
    uint16_t methodCount = 0;

    // Constructor: every v0 class gets exactly one, either the user's
    // (with an implicit super() prepended) or a synthesized public no-arg
    // one — see CLAUDE.md.
    if (c.constructors.empty()) {
        MethodCodeGen gen(pool, classTable, className, superBinary, /*isStatic=*/false,
                           Type::primitive(Type::Kind::Void));
        gen.emitImplicitSuperCall();
        gen.emitVoidReturn();
        writeMethodInfo(methodsBuf, pool, codeAttrNameIdx, kAccPublic, "<init>", "()V", gen.code(),
                         gen.maxStack(), /*maxLocals=*/1);
    } else {
        const ConstructorDecl& ctor = c.constructors.front();
        MethodCodeGen gen(pool, classTable, className, superBinary, /*isStatic=*/false,
                           Type::primitive(Type::Kind::Void));
        gen.emitImplicitSuperCall();
        if (ctor.body) gen.genStatements(ctor.body->stmts);
        gen.emitVoidReturn();
        std::string desc =
            methodDescriptor(paramTypesOf(ctor.params), Type::primitive(Type::Kind::Void), className);
        writeMethodInfo(methodsBuf, pool, codeAttrNameIdx, ctor.isPublic ? kAccPublic : kAccPrivate,
                         "<init>", desc, gen.code(), gen.maxStack(), ctor.maxLocals);
    }
    methodCount++;

    for (const auto& m : c.methods) {
        MethodCodeGen gen(pool, classTable, className, superBinary, m.isStatic, m.returnType);
        if (m.body) gen.genStatements(m.body->stmts);
        if (m.returnType.kind == Type::Kind::Void) gen.emitVoidReturn();
        std::string desc = methodDescriptor(paramTypesOf(m.params), m.returnType, className);
        uint16_t flags = (m.isPublic ? kAccPublic : kAccPrivate) | (m.isStatic ? kAccStatic : 0);
        writeMethodInfo(methodsBuf, pool, codeAttrNameIdx, flags, m.name, desc, gen.code(),
                         gen.maxStack(), m.maxLocals);
        methodCount++;
    }

    ByteWriter out;
    out.writeU4(0xCAFEBABE);
    out.writeU2(0);   // minor_version
    out.writeU2(49);  // major_version — see docs/subset-v0.md
    out.writeU2(pool.count());
    pool.serializeTo(out);
    out.writeU2(kAccPublic | kAccSuper);
    out.writeU2(thisClassIdx);
    out.writeU2(superClassIdx);
    out.writeU2(0);  // interfaces_count
    out.writeU2(fieldCount);
    out.writeBytes(fieldsBuf);
    out.writeU2(methodCount);
    out.writeBytes(methodsBuf);
    out.writeU2(0);  // attributes_count (no SourceFile attribute in v0)
    return out.bytes();
}

}  // namespace jc
