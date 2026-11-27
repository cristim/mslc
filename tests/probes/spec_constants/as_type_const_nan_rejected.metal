// EXPECT: error payload is not kept
//
// as_type<half>(0xFE01) in a constant is a NaN whose payload mslc's double folding does not keep,
// so it is refused rather than folded to a different pattern.
constant ushort a = 0xFE01;
constant half b = as_type<half>(a);
kernel void as_type_const_nan_rejected(device half* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    out[i] = b;
}
