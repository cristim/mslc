// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 4
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 32
// DISASM-MATCH: OpDecorate %_arr__arr_v3float_uint_2[_0-9]*_uint_2[_0-9]* ArrayStride 32
// DISASM-MATCH: OpDecorate %_arr_v3float_uint_2[_0-9]* ArrayStride 16
//
// A struct with two-dimensional array members copied from a constant reference
// into a local and out to a device buffer: the two forms differ at every
// dimension, so the copy has to rebuild each inner array. Without that
// recursion the module does not validate.
struct G
{
    uint n;
    float m[2][3];
    float3 v[2][2];
};

kernel void array_member_two_dimension_copy(device G *d [[buffer(0)]],
    constant G &g [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    G l = g;
    d[0] = l;
}
