// EXPECT: valid
// DISASM-MATCH: = OpLoad %v4float %[_0-9a-zA-Z]+ Aligned 16
// DISASM-MATCH: = OpVectorShuffle %v3float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 0 1 2[^ _0-9a-zA-Z]
//
// vertices[vid].normal.xyz in test/lighting: the field is loaded, then swizzled.
struct Vertex {
    float4 position;
    float4 normal;
};
kernel void swizzle_of_a_struct_field(device float3 *out [[buffer(0)]], constant Vertex *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = v[i].normal.xyz;
}
