// EXPECT: valid
// DISASM-MATCH: OpVectorShuffle %v3float %[0-9]+ %[0-9]+ 3 1 4
// DISASM-MATCH: OpStore %[0-9]+ %[0-9]+ Aligned 4
//
// A store to some lanes of a packed vector rewrites that member only: the vector
// is loaded, shuffled and stored back at 4-byte alignment.
struct Packed
{
    float lead;
    packed_float3 value;
};

kernel void packed_swizzle_store(device Packed *out [[buffer(0)]],
                                 uint index [[thread_position_in_grid]])
{
    out[index].value.xz = float2(1.0, 3.0);
}
