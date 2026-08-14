#pragma once

#include <cstdint>

// Named JVM opcodes used by v0 codegen. See JVM SE 8 spec, section 6.5:
// https://docs.oracle.com/javase/specs/jvms/se8/html/jvms-6.html
namespace jc::op {

constexpr uint8_t nop = 0x00;

constexpr uint8_t iconst_m1 = 0x02;
constexpr uint8_t iconst_0 = 0x03;  // iconst_<i> = iconst_0 + i, i in 0..5
constexpr uint8_t dconst_0 = 0x0e;
constexpr uint8_t dconst_1 = 0x0f;
constexpr uint8_t bipush = 0x10;
constexpr uint8_t sipush = 0x11;
constexpr uint8_t ldc = 0x12;
constexpr uint8_t ldc_w = 0x13;
constexpr uint8_t ldc2_w = 0x14;

constexpr uint8_t iload = 0x15;
constexpr uint8_t dload = 0x18;
constexpr uint8_t aload = 0x19;
constexpr uint8_t iload_0 = 0x1a;  // iload_<n> = iload_0 + n, n in 0..3
constexpr uint8_t dload_0 = 0x26;  // dload_<n> = dload_0 + n
constexpr uint8_t aload_0 = 0x2a;  // aload_<n> = aload_0 + n

constexpr uint8_t istore = 0x36;
constexpr uint8_t dstore = 0x39;
constexpr uint8_t astore = 0x3a;
constexpr uint8_t istore_0 = 0x3b;  // istore_<n> = istore_0 + n
constexpr uint8_t dstore_0 = 0x47;  // dstore_<n> = dstore_0 + n
constexpr uint8_t astore_0 = 0x4b;  // astore_<n> = astore_0 + n

constexpr uint8_t pop = 0x57;
constexpr uint8_t pop2 = 0x58;
constexpr uint8_t dup = 0x59;
constexpr uint8_t dup_x1 = 0x5a;
constexpr uint8_t dup2 = 0x5c;
constexpr uint8_t dup2_x1 = 0x5d;

constexpr uint8_t iadd = 0x60;
constexpr uint8_t dadd = 0x63;
constexpr uint8_t isub = 0x64;
constexpr uint8_t dsub = 0x67;
constexpr uint8_t imul = 0x68;
constexpr uint8_t dmul = 0x6b;
constexpr uint8_t idiv = 0x6c;
constexpr uint8_t ddiv = 0x6f;
constexpr uint8_t irem = 0x70;
constexpr uint8_t drem = 0x73;
constexpr uint8_t ineg = 0x74;
constexpr uint8_t dneg = 0x77;

constexpr uint8_t ixor = 0x82;
constexpr uint8_t i2d = 0x87;

constexpr uint8_t dcmpg = 0x98;

constexpr uint8_t ifeq = 0x99;
constexpr uint8_t ifne = 0x9a;
constexpr uint8_t iflt = 0x9b;
constexpr uint8_t ifge = 0x9c;
constexpr uint8_t ifgt = 0x9d;
constexpr uint8_t ifle = 0x9e;
constexpr uint8_t if_icmpeq = 0x9f;
constexpr uint8_t if_icmpne = 0xa0;
constexpr uint8_t if_icmplt = 0xa1;
constexpr uint8_t if_icmpge = 0xa2;
constexpr uint8_t if_icmpgt = 0xa3;
constexpr uint8_t if_icmple = 0xa4;
constexpr uint8_t if_acmpeq = 0xa5;
constexpr uint8_t if_acmpne = 0xa6;
constexpr uint8_t goto_ = 0xa7;

constexpr uint8_t ireturn = 0xac;
constexpr uint8_t dreturn = 0xaf;
constexpr uint8_t areturn = 0xb0;
constexpr uint8_t return_ = 0xb1;
constexpr uint8_t getstatic = 0xb2;
constexpr uint8_t putfield = 0xb5;
constexpr uint8_t getfield = 0xb4;
constexpr uint8_t invokevirtual = 0xb6;
constexpr uint8_t invokespecial = 0xb7;
constexpr uint8_t invokestatic = 0xb8;
constexpr uint8_t new_ = 0xbb;

}  // namespace jc::op
