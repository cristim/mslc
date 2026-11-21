// EXPECT: error a long is not an index
constexpr constant ulong K = 2;
kernel void attribute_index_ulong_constant_rejected(device float *out [[buffer(K)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = 1.0;
}
