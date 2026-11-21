// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 8
// DISASM-MATCH: OpDecorate %_arr_ulong_uint_8[_0-9]* ArrayStride 8
//
// A ulong array member is aligned to 8 even after a uint, and steps by 8.
struct Args
{
    uint ne;
    ulong o1[8];
};

kernel void array_member_wide_scalar_layout(device ulong *out [[buffer(0)]],
    constant Args &args [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = args.o1[i & 7] + args.ne;
}
