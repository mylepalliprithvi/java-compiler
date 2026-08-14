#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace jc {

// Growable big-endian byte buffer, used both for the constant pool and for
// method Code arrays. patchU2 supports backpatching branch offsets once
// their target address is known.
class ByteWriter {
public:
    void writeU1(uint8_t v) { bytes_.push_back(v); }

    void writeU2(uint16_t v) {
        bytes_.push_back(static_cast<uint8_t>(v >> 8));
        bytes_.push_back(static_cast<uint8_t>(v & 0xff));
    }

    void writeU4(uint32_t v) {
        bytes_.push_back(static_cast<uint8_t>(v >> 24));
        bytes_.push_back(static_cast<uint8_t>(v >> 16));
        bytes_.push_back(static_cast<uint8_t>(v >> 8));
        bytes_.push_back(static_cast<uint8_t>(v & 0xff));
    }

    void writeBytes(const std::vector<uint8_t>& b) { bytes_.insert(bytes_.end(), b.begin(), b.end()); }
    void writeBytes(const ByteWriter& other) { writeBytes(other.bytes_); }

    void patchU2(size_t pos, uint16_t v) {
        bytes_[pos] = static_cast<uint8_t>(v >> 8);
        bytes_[pos + 1] = static_cast<uint8_t>(v & 0xff);
    }

    size_t size() const { return bytes_.size(); }
    const std::vector<uint8_t>& bytes() const { return bytes_; }

private:
    std::vector<uint8_t> bytes_;
};

}  // namespace jc
