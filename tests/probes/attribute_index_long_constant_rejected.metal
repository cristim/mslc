// EXPECT: error a long is not an index
//
// Apple: "invalid type 'long' for attribute index expression", for a named constant too.
constexpr constant long K = 2;
kernel void attribute_index_long_constant_rejected(device float *out [[buffer(K)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = 1.0;
}
