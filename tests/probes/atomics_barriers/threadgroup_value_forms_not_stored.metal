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

kernel void threadgroup_value_forms_not_stored(device const float *in [[buffer(0)]],
    device float *out [[buffer(1)]], uint i [[thread_index_in_threadgroup]])
{
    threadgroup float f;
    threadgroup float4 v;
    threadgroup P p;
    threadgroup float4x4 m;
    threadgroup atomic_uint c;
    if (i == 0) {
        f = in[0];
        v = float4(in[1]);
        p.a = in[2];
        m[1] = float4(in[3]);
        atomic_store_explicit(&c, 5u, memory_order_relaxed);
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    out[i] = f + v.x + p.a + m[1].x + float(atomic_load_explicit(&c, memory_order_relaxed));
}
