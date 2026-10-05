// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 4 5 2 3[^ _0-9a-zA-Z]
//
// A member of a local struct is stored through its own address.
struct VertexOut {
    float4 position;
    float4 colour;
};

kernel void swizzle_store_struct_member(device float4 *out [[buffer(0)]], constant float2 *b [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    VertexOut v;
    v.position = float4(0.0, 0.0, 0.0, 1.0);
    v.colour = float4(1.0);
    v.position.xy = b[i];
    out[i] = v.position;
}
