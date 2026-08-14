#include "codegen/ConstantPool.hpp"

#include <cstring>

namespace jc {

namespace {
constexpr uint8_t kUtf8 = 1;
constexpr uint8_t kInteger = 3;
constexpr uint8_t kDouble = 6;
constexpr uint8_t kClass = 7;
constexpr uint8_t kString = 8;
constexpr uint8_t kFieldref = 9;
constexpr uint8_t kMethodref = 10;
constexpr uint8_t kNameAndType = 12;
}  // namespace

uint16_t ConstantPool::reserve(uint16_t slots) {
    uint16_t idx = nextIndex_;
    nextIndex_ += slots;
    return idx;
}

uint16_t ConstantPool::addUtf8(const std::string& s) {
    auto it = utf8_.find(s);
    if (it != utf8_.end()) return it->second;

    ByteWriter w;
    w.writeU1(kUtf8);
    w.writeU2(static_cast<uint16_t>(s.size()));
    for (unsigned char c : s) w.writeU1(c);

    uint16_t idx = reserve(1);
    entries_.push_back(w.bytes());
    utf8_[s] = idx;
    return idx;
}

uint16_t ConstantPool::addClass(const std::string& binaryName) {
    auto it = classes_.find(binaryName);
    if (it != classes_.end()) return it->second;

    uint16_t nameIdx = addUtf8(binaryName);
    ByteWriter w;
    w.writeU1(kClass);
    w.writeU2(nameIdx);

    uint16_t idx = reserve(1);
    entries_.push_back(w.bytes());
    classes_[binaryName] = idx;
    return idx;
}

uint16_t ConstantPool::addString(const std::string& value) {
    auto it = strings_.find(value);
    if (it != strings_.end()) return it->second;

    uint16_t utf8Idx = addUtf8(value);
    ByteWriter w;
    w.writeU1(kString);
    w.writeU2(utf8Idx);

    uint16_t idx = reserve(1);
    entries_.push_back(w.bytes());
    strings_[value] = idx;
    return idx;
}

uint16_t ConstantPool::addInteger(int32_t value) {
    auto it = integers_.find(value);
    if (it != integers_.end()) return it->second;

    ByteWriter w;
    w.writeU1(kInteger);
    w.writeU4(static_cast<uint32_t>(value));

    uint16_t idx = reserve(1);
    entries_.push_back(w.bytes());
    integers_[value] = idx;
    return idx;
}

uint16_t ConstantPool::addDouble(double value) {
    auto it = doubles_.find(value);
    if (it != doubles_.end()) return it->second;

    uint64_t bits;
    static_assert(sizeof(bits) == sizeof(value), "double must be 8 bytes");
    std::memcpy(&bits, &value, sizeof(bits));

    ByteWriter w;
    w.writeU1(kDouble);
    w.writeU4(static_cast<uint32_t>(bits >> 32));
    w.writeU4(static_cast<uint32_t>(bits & 0xffffffffu));

    uint16_t idx = reserve(2);  // occupies two index slots
    entries_.push_back(w.bytes());
    doubles_[value] = idx;
    return idx;
}

uint16_t ConstantPool::addNameAndType(const std::string& name, const std::string& descriptor) {
    std::string key = name + "|" + descriptor;
    auto it = namesAndTypes_.find(key);
    if (it != namesAndTypes_.end()) return it->second;

    uint16_t nameIdx = addUtf8(name);
    uint16_t descIdx = addUtf8(descriptor);
    ByteWriter w;
    w.writeU1(kNameAndType);
    w.writeU2(nameIdx);
    w.writeU2(descIdx);

    uint16_t idx = reserve(1);
    entries_.push_back(w.bytes());
    namesAndTypes_[key] = idx;
    return idx;
}

uint16_t ConstantPool::addFieldref(const std::string& className, const std::string& name,
                                    const std::string& descriptor) {
    std::string key = className + "|" + name + "|" + descriptor;
    auto it = fieldrefs_.find(key);
    if (it != fieldrefs_.end()) return it->second;

    uint16_t classIdx = addClass(className);
    uint16_t natIdx = addNameAndType(name, descriptor);
    ByteWriter w;
    w.writeU1(kFieldref);
    w.writeU2(classIdx);
    w.writeU2(natIdx);

    uint16_t idx = reserve(1);
    entries_.push_back(w.bytes());
    fieldrefs_[key] = idx;
    return idx;
}

uint16_t ConstantPool::addMethodref(const std::string& className, const std::string& name,
                                     const std::string& descriptor) {
    std::string key = className + "|" + name + "|" + descriptor;
    auto it = methodrefs_.find(key);
    if (it != methodrefs_.end()) return it->second;

    uint16_t classIdx = addClass(className);
    uint16_t natIdx = addNameAndType(name, descriptor);
    ByteWriter w;
    w.writeU1(kMethodref);
    w.writeU2(classIdx);
    w.writeU2(natIdx);

    uint16_t idx = reserve(1);
    entries_.push_back(w.bytes());
    methodrefs_[key] = idx;
    return idx;
}

void ConstantPool::serializeTo(ByteWriter& out) const {
    for (const auto& entry : entries_) {
        for (uint8_t b : entry) out.writeU1(b);
    }
}

}  // namespace jc
