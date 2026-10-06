// EXPECT: error multi-word type name is not supported yet
kernel void cast_to_unsigned_diagnosed(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    out[i] = (unsigned)i;
}
