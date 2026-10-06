// EXPECT: valid
// DISASM-MATCH: = OpIAdd %int %[0-9]+ %int_5
//
// *p += 5 is p[0] += 5.
kernel void compound_through_a_dereference(device int *out [[buffer(0)]],
                                           uint i [[thread_position_in_grid]])
{
    *out += 5;
}
