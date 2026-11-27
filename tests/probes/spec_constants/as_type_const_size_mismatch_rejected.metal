// EXPECT: error needs an operand of the same size
//
// as_type<uint>(1.0h) in a constant initialiser is a 16-bit operand for a 32-bit type, which is rejected.
constant uint u0 = as_type<uint>(1.0h);
kernel void as_type_const_size_mismatch_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    out[i] = u0;
}
