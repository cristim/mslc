// EXPECT: error [[attribute]] needs an index
//
// [[attribute]] without an index says where nothing goes, so it is rejected
// rather than read as attribute 0. A vertex input at the wrong location reads
// the wrong vertex buffer field, which is silent, so the argument is required
// instead of defaulted.
struct Vertex
{
    float4 position [[position]];
    float4 color [[attribute]];
};

kernel void struct_field_attribute_needs_index(device float* out [[buffer(0)]],
                                               uint i [[thread_position_in_grid]])
{
    out[i] = 1.0;
}
