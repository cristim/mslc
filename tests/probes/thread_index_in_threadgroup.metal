// EXPECT: valid
// DISASM: BuiltIn LocalInvocationIndex
// DISASM: OpVariable %_ptr_Input_uint Input
// DISASM-NOT: OpVariable %_ptr_Input_v3uint Input
//
// [[thread_index_in_threadgroup]] is the linear index of the invocation within
// its workgroup, and lowers to LocalInvocationIndex.
//
// The two type needles are the point of this probe. Vulkan fixes the type of
// every built-in input, and LocalInvocationIndex is a scalar uint, unlike the
// compute position ids (GlobalInvocationId, LocalInvocationId, WorkgroupId),
// which are uvec3 even where MSL declares them narrower. The lowering switch
// picks the type by falling through to a uvec3 default, so a missing case here
// does not fail to compile: it silently declares the input as a uvec3. A
// shader that then reads it as a uint either fails validation or, worse, reads
// the wrong thing.
//
// So this probe carries only this one built-in parameter, which makes
// "no uvec3 Input variable at all" a true statement, and the DISASM-NOT above
// fails if the case is ever removed. A probe with only the DISASM line would
// pass either way and pin nothing.
#include <metal_stdlib>
using namespace metal;

kernel void thread_index_in_threadgroup(device float *d [[buffer(0)]],
                                        uint index [[thread_index_in_threadgroup]])
{
    d[index] = (float)index;
}
