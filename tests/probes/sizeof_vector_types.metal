// EXPECT: valid
// DISASM: OpTypeArray %float %uint_1002
// DISASM: OpTypeArray %float %uint_2004
// DISASM: OpTypeArray %float %uint_3004
// DISASM: OpTypeArray %float %uint_4008
// DISASM: OpTypeArray %float %uint_5004
// DISASM: OpTypeArray %float %uint_6008
// DISASM: OpTypeArray %float %uint_7008
// DISASM: OpTypeArray %float %uint_8008
// DISASM: OpTypeArray %float %uint_9016
// DISASM: OpTypeArray %float %uint_10016
// DISASM: OpTypeArray %float %uint_11008
// DISASM: OpTypeArray %float %uint_12016
// DISASM: OpTypeArray %float %uint_13016
// DISASM: OpTypeArray %float %uint_14016
// DISASM: OpTypeArray %float %uint_15032
// DISASM: OpTypeArray %float %uint_16002
// DISASM: OpTypeArray %float %uint_17004
// DISASM: OpTypeArray %float %uint_18004
// DISASM: OpTypeArray %float %uint_19012
// DISASM: OpTypeArray %float %uint_20006
// DISASM: OpTypeArray %float %uint_21003
// DISASM: OpTypeArray %float %uint_22016
// DISASM: OpTypeArray %float %uint_23002
//
// A vector of three is as big as one of four, and a packed one is its components back to back.
// Each size is read back as the length of a distinct array (the size plus 1000 times its
// position), so a wrong size for any one type changes exactly one array. Apple's compiler is the
// oracle for every figure (xcrun metal -S -emit-llvm over the same sizeofs).
#include <metal_stdlib>
using namespace metal;
#define CHECK(k, T) float a##k[sizeof(T) + 1000 * k]; a##k[0] = 1.0f; sum += a##k[0];
kernel void sizeof_vector_types(device float *out [[buffer(0)]])
{
    float sum = 0.0f;
    CHECK(1, char2)
    CHECK(2, char3)
    CHECK(3, char4)
    CHECK(4, short3)
    CHECK(5, half2)
    CHECK(6, half3)
    CHECK(7, half4)
    CHECK(8, int2)
    CHECK(9, int3)
    CHECK(10, int4)
    CHECK(11, float2)
    CHECK(12, float3)
    CHECK(13, float4)
    CHECK(14, long2)
    CHECK(15, long3)
    CHECK(16, bool2)
    CHECK(17, bool3)
    CHECK(18, bool4)
    CHECK(19, packed_float3)
    CHECK(20, packed_half3)
    CHECK(21, packed_char3)
    CHECK(22, packed_int4)
    CHECK(23, packed_uchar2)
    out[0] = sum;
}
