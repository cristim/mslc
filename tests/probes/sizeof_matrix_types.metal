// EXPECT: valid
// DISASM: OpTypeArray %float %uint_1016
// DISASM: OpTypeArray %float %uint_2032
// DISASM: OpTypeArray %float %uint_3032
// DISASM: OpTypeArray %float %uint_4024
// DISASM: OpTypeArray %float %uint_5048
// DISASM: OpTypeArray %float %uint_6048
// DISASM: OpTypeArray %float %uint_7064
// DISASM: OpTypeArray %float %uint_8064
// DISASM: OpTypeArray %float %uint_9008
// DISASM: OpTypeArray %float %uint_10024
// DISASM: OpTypeArray %float %uint_11032
// DISASM: OpTypeArray %float %uint_12012
//
// A matrix is its columns as an array of column vectors, so a three-row column takes four rows.
// Each size is read back as the length of a distinct array (the size plus 1000 times its
// position), so a wrong size for any one type changes exactly one array. Apple's compiler is the
// oracle for every figure (xcrun metal -S -emit-llvm over the same sizeofs).
#include <metal_stdlib>
using namespace metal;
#define CHECK(k, T) float a##k[sizeof(T) + 1000 * k]; a##k[0] = 1.0f; sum += a##k[0];
kernel void sizeof_matrix_types(device float *out [[buffer(0)]])
{
    float sum = 0.0f;
    CHECK(1, float2x2)
    CHECK(2, float2x3)
    CHECK(3, float2x4)
    CHECK(4, float3x2)
    CHECK(5, float3x3)
    CHECK(6, float3x4)
    CHECK(7, float4x4)
    CHECK(8, float4x3)
    CHECK(9, half2x2)
    CHECK(10, half3x3)
    CHECK(11, half4x3)
    CHECK(12, half3x2)
    out[0] = sum;
}
