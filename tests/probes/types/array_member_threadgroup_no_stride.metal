// EXPECT: valid
// DISASM-NO-MATCH: OpDecorate %_arr_float_uint_4[_0-9]* ArrayStride
// DISASM-NO-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset
// DISASM-MATCH: OpTypePointer Workgroup %_struct_[0-9]+
//
// A threadgroup struct has no explicit layout: ArrayStride and Offset are
// forbidden on a Workgroup variable, so the array member is a plain array.
struct Tile
{
    uint count;
    float values[4];
};

kernel void array_member_threadgroup_no_stride(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    threadgroup Tile t;
    t.values[i & 3] = 1.0f;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    out[0] = t.values[0];
}
