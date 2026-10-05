// EXPECT: error indexing an expression is not supported yet
//
// Apple allows a packed vector to be subscripted. mslc does not subscript any
// vector yet, packed or not, and says so rather than reading the wrong bytes.
struct Packed
{
    packed_float3 value;
};

kernel void packed_subscript_rejected(device float *out [[buffer(0)]],
                                      device const Packed *in [[buffer(1)]],
                                      uint index [[thread_position_in_grid]])
{
    out[index] = in[index].value[1];
}
