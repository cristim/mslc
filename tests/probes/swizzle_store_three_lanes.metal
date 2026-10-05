// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v3float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 4 1 3[^ _0-9a-zA-Z]
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ Aligned 16[^ _0-9a-zA-Z]
//
// A float3 element is 16 bytes wide, and storing one writes back only its three
// lanes.
kernel void swizzle_store_three_lanes(device float3 *out [[buffer(0)]], constant float2 *b [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i].zx = b[i];
}
