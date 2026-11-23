// EXPECT: error needs an operand of the same size as uint
//
// Apple rejects as_type<uint>(half): the sizes differ.
kernel void as_type_size_mismatch_rejected(device uint* out [[buffer(0)]], device half* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = as_type<uint>(in[i]);
}
