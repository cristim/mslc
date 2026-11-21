// EXPECT: valid
// REFLECT: "metal_index": 1
//
// An attribute index may name an integer constant, as Apple allows.
constant int kOut = 1;
kernel void attribute_index_named_constant(device float *out [[buffer(kOut)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = 1.0;
}
