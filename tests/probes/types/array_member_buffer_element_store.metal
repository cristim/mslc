// EXPECT: valid
// DISASM-MATCH: OpDecorate %_runtimearr__struct_[0-9]+ ArrayStride 48
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 16
// DISASM-MATCH: OpDecorate %_arr_float_uint_8[_0-9]* ArrayStride 4
//
// An element of a device buffer of structs holds an array member that is written:
// float4 head, then float w[8] (32 bytes), 48 in all. The array is a member of
// the element and has ArrayStride on the array, not on the runtime array of
// elements.
struct Cell
{
    float4 head;
    float w[8];
};

kernel void array_member_buffer_element_store(device Cell *cells [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    cells[i].w[i & 7] = cells[i].head.x;
    cells[i].w[3] += 1.0f;
}
