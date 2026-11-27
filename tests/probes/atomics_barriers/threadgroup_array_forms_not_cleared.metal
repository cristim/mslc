// EXPECT: valid
// DISASM: OpVariable %_ptr_Workgroup
// DISASM-NOT: OpConstantNull
// Metal leaves threadgroup memory uninitialised and every invocation reaches the
// declaration, so a zero-initialising store (OpStore of OpConstantNull to the
// Workgroup variable) erases what other invocations wrote before the barrier.
// Issue #364. Nothing else in this kernel needs a null constant.
#include <metal_stdlib>
using namespace metal;
struct P { float a; float b; };

kernel void threadgroup_array_forms_not_cleared(device const float *in [[buffer(0)]],
    device float *out [[buffer(1)]], uint i [[thread_index_in_threadgroup]],
    uint n [[threads_per_threadgroup]])
{
    threadgroup float grid2[4][4];
    threadgroup P pairs[8];
    threadgroup float4 vecs[8];
    threadgroup float4x4 mats[2];
    for (uint pass = 0; pass < 2; ++pass) {
        threadgroup float inLoop[8];
        inLoop[i & 7] = in[i & 7];
        threadgroup_barrier(mem_flags::mem_threadgroup);
        if (i == 0) { out[pass] = inLoop[1]; }
    }
    if (n > 8) {
        threadgroup float inBranch[8];
        inBranch[i & 7] = in[i & 7];
        threadgroup_barrier(mem_flags::mem_threadgroup);
        out[2] = inBranch[3];
    }
    switch (n) {
    case 64: {
        threadgroup float inSwitch[8];
        inSwitch[i & 7] = in[i & 7];
        threadgroup_barrier(mem_flags::mem_threadgroup);
        out[3] = inSwitch[2];
        break;
    }
    default:
        break;
    }
    grid2[i & 3][(i >> 2) & 3] = in[(i & 3) + 4 * ((i >> 2) & 3)];
    pairs[i & 7].a = in[i & 7];
    vecs[i & 7] = float4(in[i & 7]);
    mats[i & 1] = float4x4(in[i & 1]);
    threadgroup_barrier(mem_flags::mem_threadgroup);
    out[4] = grid2[1][1] + pairs[1].a + vecs[2].x + mats[0][0][0];
}
