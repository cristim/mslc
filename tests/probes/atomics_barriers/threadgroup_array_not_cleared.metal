// EXPECT: valid
// DISASM: OpVariable %_ptr_Workgroup
// DISASM-NOT: OpConstantNull
// Metal leaves threadgroup memory uninitialised and every invocation reaches the
// declaration, so a zero-initialising store (OpStore of OpConstantNull to the
// Workgroup variable) erases what other invocations wrote before the barrier.
// Issue #364. Nothing else in this kernel needs a null constant.
#include <metal_stdlib>
using namespace metal;
kernel void threadgroup_array_not_cleared(device const float *in [[buffer(0)]],
    device float *out [[buffer(1)]], uint i [[thread_index_in_threadgroup]])
{
    threadgroup float tg[64];
    tg[i] = in[i];
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (i == 0) { out[0] = tg[0]; out[1] = tg[5]; }
}
