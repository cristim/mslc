// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 16
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 32
// DISASM-MATCH: OpDecorate %_runtimearr__struct_[0-9]+ ArrayStride 40
//
// The vertex layout of indium's texturing sample: 16 + 16 + 8 bytes with the
// alignment of a float, so an array of them steps 40.
struct Vertex
{
    packed_float4 position;
    packed_float4 normal;
    packed_float2 texCoords;
};

kernel void packed_vertex_struct_layout(device float4 *out [[buffer(0)]],
                                        const device Vertex *vertices [[buffer(1)]],
                                        uint index [[thread_position_in_grid]])
{
    float4 position = vertices[index].position;
    out[index] = position + vertices[index].normal;
    out[index].xy = vertices[index].texCoords;
}
