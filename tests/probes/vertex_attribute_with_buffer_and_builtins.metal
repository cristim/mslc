// EXPECT: valid
// DISASM-MATCH: OpDecorate %[0-9]+ Location 2
// DISASM: BuiltIn VertexIndex
// DISASM: BuiltIn InstanceIndex
// DISASM: BuiltIn Position
// REFLECT: { "kind": "VertexInput", "metal_index": 2, "location": 2, "name": "p" }
// REFLECT: { "kind": "Buffer", "metal_index": 1, "descriptor": { "set": 0, "binding": 0 }, "member": 0, "param_index": 1, "name": "scale" }
//
// The [[stage_in]] struct sits beside a buffer and the vertex_id and instance_id
// builtins in one function; each keeps its own binding.
#include <metal_stdlib>
using namespace metal;
struct In { float4 p [[attribute(2)]]; };
struct Out { float4 p [[position]]; };
vertex Out vertex_attribute_with_buffer_and_builtins(In in [[stage_in]],
    constant float* scale [[buffer(1)]], uint vid [[vertex_id]], uint iid [[instance_id]])
{ Out o; o.p = in.p * scale[0] * float(vid + iid); return o; }
