// EXPECT: valid
// REFLECT: "metal_index": 2
//
// Apple takes a named bool constant as an attribute index (true is 1, so K + 1 is 2) and
// refuses only a bool literal in it.
constexpr constant bool K = true;
kernel void attribute_index_named_bool_constant(device float *out [[buffer(K + 1)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = 1.0;
}
