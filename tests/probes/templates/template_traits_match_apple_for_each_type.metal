// EXPECT: valid
// RUN: grid=1 local=1
// DISASM: %int_53 = OpConstant %int 53
// DISASM: %int_54 = OpConstant %int 54
// DISASM: %int_57 = OpConstant %int 57
// DISASM: %int_58 = OpConstant %int 58
// DISASM: %int_86 = OpConstant %int 86
// DISASM: %int_90 = OpConstant %int 90
// DISASM: %int_128 = OpConstant %int 128
// DISASM: %int_245 = OpConstant %int 245
// DISASM: %int_777 = OpConstant %int 777
// DISASM: %int_555 = OpConstant %int 555
// DISASM: %int_333 = OpConstant %int 333
// Refs #341: the <type_traits> predicates, as Apple evaluates them for each type mslc has, folded
// to file-scope constants. A vector is floating point, integral, signed or unsigned by its element
// and is arithmetic; a matrix or a struct is only a class; an enum is only a scalar; bool is
// unsigned. CODE sums 1 floating point, 2 integral, 4 scalar, 8 vector, 16 arithmetic, 32 signed,
// 64 unsigned, 128 class. Every row was compared with Apple's compiler (diffcheck, RUN above).
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
enum En { e0, e1 };
#define CODE(T) ((is_floating_point<T>::value ? 1 : 0) + (is_integral<T>::value ? 2 : 0) \
	+ (is_scalar<T>::value ? 4 : 0) + (is_vector<T>::value ? 8 : 0) + (is_arithmetic<T>::value ? 16 : 0) \
	+ (is_signed<T>::value ? 32 : 0) + (is_unsigned<T>::value ? 64 : 0) + (is_class<T>::value ? 128 : 0))
constant int k_bool = CODE(bool);
constant int k_char = CODE(char);
constant int k_uchar = CODE(uchar);
constant int k_short = CODE(short);
constant int k_ushort = CODE(ushort);
constant int k_int = CODE(int);
constant int k_uint = CODE(uint);
constant int k_long = CODE(long);
constant int k_ulong = CODE(ulong);
constant int k_half = CODE(half);
constant int k_float = CODE(float);
constant int k_float2 = CODE(float2);
constant int k_float3 = CODE(float3);
constant int k_int2 = CODE(int2);
constant int k_uint4 = CODE(uint4);
constant int k_half4 = CODE(half4);
constant int k_bool2 = CODE(bool2);
constant int k_float3x3 = CODE(float3x3);
constant int k_S = CODE(S);
constant int k_En = CODE(En);
constant int k_packed_float3 = CODE(packed_float3);
constant int k_constfloat = CODE(const float);
constant int kSame = (is_same<float, float>::value ? 1 : 0) + (is_same<float, float3>::value ? 2 : 0)
	+ (is_same<float3, float3>::value ? 4 : 0) + (is_same<const float, float>::value ? 8 : 0)
	+ (is_const<const float>::value ? 16 : 0) + (is_same<make_scalar_t<float3>, float>::value ? 32 : 0)
	+ (is_same_v<S, S> ? 64 : 0) + (is_enum<En>::value ? 128 : 0);
constant int kEnumScalar = is_scalar<En>::value ? 777 : 888;
constant int kBoolUnsigned = is_unsigned<bool>::value ? 555 : 666;
constant int kHalfVectorFloat = is_floating_point<half4>::value ? 333 : 444;
kernel void k(device int* out [[buffer(0)]])
{
	out[0] = k_bool;
	out[1] = k_char;
	out[2] = k_uchar;
	out[3] = k_short;
	out[4] = k_ushort;
	out[5] = k_int;
	out[6] = k_uint;
	out[7] = k_long;
	out[8] = k_ulong;
	out[9] = k_half;
	out[10] = k_float;
	out[11] = k_float2;
	out[12] = k_float3;
	out[13] = k_int2;
	out[14] = k_uint4;
	out[15] = k_half4;
	out[16] = k_bool2;
	out[17] = k_float3x3;
	out[18] = k_S;
	out[19] = k_En;
	out[20] = k_packed_float3;
	out[21] = k_constfloat;
	out[22] = kSame;
	out[23] = kEnumScalar + kBoolUnsigned + kHalfVectorFloat;
}
