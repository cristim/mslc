// EXPECT: valid
// DISASM: OpDecorate %10 Location 3
// DISASM: OpDecorate %15 Location 0
// DISASM: OpDecorate %22 Location 7
// DISASM: %10 = OpVariable %_ptr_Input_uint Input
// DISASM: %15 = OpVariable %_ptr_Input_v4float Input
// DISASM: %22 = OpVariable %_ptr_Input_v2float Input
// DISASM-MATCH: OpEntryPoint Vertex %[0-9]+ "vertex_attribute_location_is_the_index" %10 %15 %22 %gl_Position
// DISASM-NOT: Flat
// DISASM-NOT: Location 1
// REFLECT: { "kind": "VertexInput", "metal_index": 3, "location": 3, "name": "id" }
// REFLECT: { "kind": "VertexInput", "metal_index": 0, "location": 0, "name": "pos" }
// REFLECT: { "kind": "VertexInput", "metal_index": 7, "location": 7, "name": "uv" }
//
// A vertex function's [[stage_in]] field is an Input at the Location its
// [[attribute(n)]] names, whatever its place in the struct: Iridium decorates an
// air.vertex_input parameter with its air.location_index, and indium's vertex
// descriptor keys its VkVertexInputAttributeDescription locations by the same
// index. The fields are deliberately out of order and sparse. An integer vertex
// input is not interpolated, so it carries no Flat.
#include <metal_stdlib>
using namespace metal;
struct In { uint id [[attribute(3)]]; float4 pos [[attribute(0)]]; half2 uv [[attribute(7)]]; };
struct Out { float4 p [[position]]; };
vertex Out vertex_attribute_location_is_the_index(In in [[stage_in]])
{ Out o; o.p = in.pos + float4(float(in.id), float2(in.uv), 0.0); return o; }
