// EXPECT: error variables in the threadgroup address space can only be declared in a kernel function
// A threadgroup array is refused outside a kernel, exactly as a threadgroup
// scalar is. Vulkan backs this with a rule rather than a convention:
// VUID-StandaloneSpirv-None-04645 limits the Workgroup storage class to the
// compute execution model.
//
// Before this was enforced for the array form, mslc accepted the source below,
// exited 0 and wrote a module that spirv-val then rejected, which is worse than
// a diagnostic: the caller is handed an invalid artifact and told it worked.
// A diagnostic at the declaration is what makes the failure visible where it
// can be acted on.
#include <metal_stdlib>
using namespace metal;

vertex float4 threadgroup_array_in_vertex_rejected(uint vid [[vertex_id]])
{
    threadgroup float s[4];
    s[0] = 1.0f;
    return float4(s[0]);
}
