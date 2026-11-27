// EXPECT: valid
// RUN: grid=1 local=1
// DISASM: %int_53 = OpConstant %int 53
// DISASM: %int_54 = OpConstant %int 54
// DISASM: %int_57 = OpConstant %int 57
// DISASM: %int_58 = OpConstant %int 58
// DISASM: %int_86 = OpConstant %int 86
// DISASM: %int_90 = OpConstant %int 90
// DISASM: %int_128 = OpConstant %int 128
// DISASM: %int_281 = OpConstant %int 281
// DISASM: %int_282 = OpConstant %int 282
// DISASM: %int_309 = OpConstant %int 309
// DISASM: %int_310 = OpConstant %int 310
// DISASM: %int_342 = OpConstant %int 342
// DISASM: %int_384 = OpConstant %int 384
// DISASM: %int_516 = OpConstant %int 516
// DISASM: %int_772 = OpConstant %int 772
// DISASM: %int_222 = OpConstant %int 222
// Refs #341: every trait for each type, plain and const, as Apple evaluates it (each row compared
// with Apple through diffcheck). A const vector is still a vector, floating point or integral and
// arithmetic, but neither signed nor unsigned; reading its element's sign made an enable_if over
// is_signed_v pick the wrong overload. CODE: 1 floating point, 2 integral, 4 scalar, 8 vector,
// 16 arithmetic, 32 signed, 64 unsigned, 128 class, 256 const, 512 enum.
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
enum En { e0 };
#define CODE(T) ((is_floating_point_v<T> ? 1 : 0) + (is_integral_v<T> ? 2 : 0) + (is_scalar_v<T> ? 4 : 0) \
	+ (is_vector_v<T> ? 8 : 0) + (is_arithmetic_v<T> ? 16 : 0) + (is_signed_v<T> ? 32 : 0) \
	+ (is_unsigned_v<T> ? 64 : 0) + (is_class_v<T> ? 128 : 0) + (is_const_v<T> ? 256 : 0) + (is_enum_v<T> ? 512 : 0))
constant int k0 = CODE(bool);
constant int k1 = CODE(const bool);
constant int k2 = CODE(int);
constant int k3 = CODE(const int);
constant int k4 = CODE(uint);
constant int k5 = CODE(const uint);
constant int k6 = CODE(half);
constant int k7 = CODE(const half);
constant int k8 = CODE(float);
constant int k9 = CODE(const float);
constant int k10 = CODE(float3);
constant int k11 = CODE(const float3);
constant int k12 = CODE(int2);
constant int k13 = CODE(const int2);
constant int k14 = CODE(uint4);
constant int k15 = CODE(const uint4);
constant int k16 = CODE(half4);
constant int k17 = CODE(const half4);
constant int k18 = CODE(bool2);
constant int k19 = CODE(const bool2);
constant int k20 = CODE(uchar3);
constant int k21 = CODE(const uchar3);
constant int k22 = CODE(packed_float3);
constant int k23 = CODE(const packed_float3);
constant int k24 = CODE(packed_uint2);
constant int k25 = CODE(const packed_uint2);
constant int k26 = CODE(float3x3);
constant int k27 = CODE(const float3x3);
constant int k28 = CODE(S);
constant int k29 = CODE(const S);
constant int k30 = CODE(En);
constant int k31 = CODE(const En);
// bool is unsigned, and not signed: no other row tells uint from bool.
constant int kBoolSigned = is_signed<bool>::value ? 111 : 222;
kernel void k(device int* out [[buffer(0)]])
{
	out[0] = k0;
	out[1] = k1;
	out[2] = k2;
	out[3] = k3;
	out[4] = k4;
	out[5] = k5;
	out[6] = k6;
	out[7] = k7;
	out[8] = k8;
	out[9] = k9;
	out[10] = k10;
	out[11] = k11;
	out[12] = k12;
	out[13] = k13;
	out[14] = k14;
	out[15] = k15;
	out[16] = k16;
	out[17] = k17;
	out[18] = k18;
	out[19] = k19;
	out[20] = k20;
	out[21] = k21;
	out[22] = k22;
	out[23] = k23;
	out[24] = k24;
	out[25] = k25;
	out[26] = k26;
	out[27] = k27;
	out[28] = k28;
	out[29] = k29;
	out[30] = k30;
	out[31] = k31;
	out[32] = kBoolSigned;
}
