// EXPECT: valid
// DISASM: OpTypeArray %float %uint_1016
// DISASM: OpTypeArray %float %uint_2032
// DISASM: OpTypeArray %float %uint_3024
// DISASM: OpTypeArray %float %uint_4096
// DISASM: OpTypeArray %float %uint_5003
// DISASM: OpTypeArray %float %uint_6012
// DISASM: OpTypeArray %float %uint_7048
// DISASM: OpTypeArray %float %uint_8024
// DISASM: OpTypeArray %float %uint_9008
// DISASM: OpTypeArray %float %uint_10008
// DISASM: OpTypeArray %float %uint_11008
// DISASM: OpTypeArray %float %uint_12008
// DISASM: OpTypeArray %float %uint_13008
// DISASM: OpTypeArray %float %uint_14016
//
// An array steps by its element's size, so float3[2] is 32 and char3[3] is 12; any pointer is 8.
// Each size is read back as the length of a distinct array (the size plus 1000 times its
// position), so a wrong size for any one type changes exactly one array. Apple's compiler is the
// oracle for every figure (xcrun metal -S -emit-llvm over the same sizeofs).
#include <metal_stdlib>
using namespace metal;
#define CHECK(k, T) float a##k[sizeof(T) + 1000 * k]; a##k[0] = 1.0f; sum += a##k[0];
kernel void sizeof_array_and_pointer_types(device float *out [[buffer(0)]])
{
    float sum = 0.0f;
    CHECK(1, float[4])
    CHECK(2, float3[2])
    CHECK(3, float[2][3])
    CHECK(4, float3[2][3])
    CHECK(5, bool[3])
    CHECK(6, char3[3])
    CHECK(7, half3x3[2])
    CHECK(8, packed_float3[2])
    CHECK(9, device float *)
    CHECK(10, threadgroup float *)
    CHECK(11, constant float *)
    CHECK(12, thread int *)
    CHECK(13, device float3 *)
    CHECK(14, device float *[2])
    out[0] = sum;
}
