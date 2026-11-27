// EXPECT: valid
// DISASM: OpTypeArray %float %uint_1001
// DISASM: OpTypeArray %float %uint_2001
// DISASM: OpTypeArray %float %uint_3002
// DISASM: OpTypeArray %float %uint_4004
// DISASM: OpTypeArray %float %uint_5008
// DISASM: OpTypeArray %float %uint_6002
// DISASM: OpTypeArray %float %uint_7004
// DISASM: OpTypeArray %float %uint_8001
// DISASM: OpTypeArray %float %uint_9002
// DISASM: OpTypeArray %float %uint_10004
// DISASM: OpTypeArray %float %uint_11008
//
// sizeof of each scalar type: bool is one byte and long eight.
// Each size is read back as the length of a distinct array (the size plus 1000 times its
// position), so a wrong size for any one type changes exactly one array. Apple's compiler is the
// oracle for every figure (xcrun metal -S -emit-llvm over the same sizeofs).
#include <metal_stdlib>
using namespace metal;
#define CHECK(k, T) float a##k[sizeof(T) + 1000 * k]; a##k[0] = 1.0f; sum += a##k[0];
kernel void sizeof_scalar_types(device float *out [[buffer(0)]])
{
    float sum = 0.0f;
    CHECK(1, bool)
    CHECK(2, char)
    CHECK(3, short)
    CHECK(4, int)
    CHECK(5, long)
    CHECK(6, half)
    CHECK(7, float)
    CHECK(8, uchar)
    CHECK(9, ushort)
    CHECK(10, uint)
    CHECK(11, ulong)
    out[0] = sum;
}
