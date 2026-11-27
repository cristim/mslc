// EXPECT: valid
// DISASM: %_arr_float_uint_4 = OpTypeArray %float %uint_4
// DISASM: OpVariable %_ptr_Workgroup__arr_float_uint_4 Workgroup
// DISASM: OpAccessChain %_ptr_Workgroup_float
// DISASM-NOT: OpDecorate %_arr_float_uint_4 ArrayStride
//
// A threadgroup array is shared by every invocation of the workgroup, so it
// lives in Workgroup storage rather than the function's own. Apple's compiler
// accepts this and so does mslc.
//
// This probe pinned the rejection ("an array of float[4] is not lowered yet"),
// which was accurate while threadgroup arrays were refused. What it states now
// is that the declaration lowers.
//
// The DISASM-NOT is the part worth reading twice. Workgroup storage has an
// implicit layout, and VUID-StandaloneSpirv-None-10684 rejects an explicit one
// on it: "the Workgroup storage class has an explicit layout from the
// ArrayStride decoration". So the array type here must carry no ArrayStride,
// even though emitting one is the obvious thing to do and is correct for a
// buffer. The needle names the type specifically because the module does contain
// a legitimate ArrayStride, on the runtime float array inside the binding-0
// address block, and a blanket "not: ArrayStride" would pass for the wrong
// reason.
//
// The OpAccessChain needle pins the other half: an element is reached through a
// pointer to the ELEMENT, not to the array. An access chain typed at the array
// assembles and is then rejected for a result-type mismatch, which is a far
// worse place to discover it.
#include <metal_stdlib>
using namespace metal;

kernel void threadgroup_local_array(device float *d [[buffer(0)]],
                                    uint t [[thread_position_in_threadgroup]])
{
  threadgroup float s[4];
  s[t] = 1.0f;
  threadgroup_barrier(mem_flags::mem_threadgroup);
  d[0] = s[0];
}
