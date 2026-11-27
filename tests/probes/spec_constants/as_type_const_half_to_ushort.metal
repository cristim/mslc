// EXPECT: valid
// DISASM: OpConstant %ushort 15360
//
// as_type<ushort>(1.0h) in a constant initialiser folds to the half's bits, 0x3C00, as Apple does.
constant half h0 = 1.0h;
constant ushort u0 = as_type<ushort>(h0);
kernel void as_type_const_half_to_ushort(device ushort* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    out[i] = u0;
}
