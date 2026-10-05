// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 12
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 24
// DISASM-MATCH: OpTypeArray %float %uint_3
//
// The second packed_float3 starts at 12 and so straddles a 16-byte boundary,
// which spirv-val accepts for an array of floats and rejects for a vec3 unless
// scalarBlockLayout is assumed. This module validating is the claim: a packed
// vector is stored as an array of its components and read through a pointer to
// the vector.
struct Vertex
{
    packed_float3 position;
    packed_float3 normal;
    packed_float2 uv;
};

kernel void packed_adjacent_members_validate(device Vertex *out [[buffer(0)]],
                                             device const Vertex *in [[buffer(1)]],
                                             uint index [[thread_position_in_grid]])
{
    out[index].normal = in[index].position;
    out[index].uv = in[index].uv;
}
