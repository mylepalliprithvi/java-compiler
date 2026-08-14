#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "codegen/ByteWriter.hpp"

namespace jc {

// JVM constant pool builder. Entries are deduped by content and indices are
// assigned in add order. Double constants occupy two consecutive index slots
// per the classfile format (JVM SE 8 spec 4.4.5) even though only one is
// ever emitted — the caller never sees the difference, addDouble() just
// returns the usable index.
class ConstantPool {
public:
    uint16_t addUtf8(const std::string& s);
    uint16_t addClass(const std::string& binaryName);        // e.g. "java/lang/Object"
    uint16_t addString(const std::string& value);             // CONSTANT_String, for string literals
    uint16_t addInteger(int32_t value);
    uint16_t addDouble(double value);
    uint16_t addNameAndType(const std::string& name, const std::string& descriptor);
    uint16_t addFieldref(const std::string& className, const std::string& name,
                          const std::string& descriptor);
    uint16_t addMethodref(const std::string& className, const std::string& name,
                           const std::string& descriptor);

    // constant_pool_count (index range is [1, count)).
    uint16_t count() const { return nextIndex_; }

    // Appends every entry, in add order, to `out` (constant_pool[] body,
    // without the leading count).
    void serializeTo(ByteWriter& out) const;

private:
    uint16_t reserve(uint16_t slots);

    std::vector<std::vector<uint8_t>> entries_;
    uint16_t nextIndex_ = 1;

    std::unordered_map<std::string, uint16_t> utf8_;
    std::unordered_map<std::string, uint16_t> classes_;
    std::unordered_map<std::string, uint16_t> strings_;
    std::unordered_map<int32_t, uint16_t> integers_;
    std::unordered_map<double, uint16_t> doubles_;
    std::unordered_map<std::string, uint16_t> namesAndTypes_;
    std::unordered_map<std::string, uint16_t> fieldrefs_;
    std::unordered_map<std::string, uint16_t> methodrefs_;
};

}  // namespace jc
