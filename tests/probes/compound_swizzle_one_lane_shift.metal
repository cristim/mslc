// EXPECT: valid
// DISASM-MATCH: = OpCompositeExtract %int %[0-9]+ 2
// DISASM-MATCH: = OpShiftLeftLogical %int %[0-9]+ %int_3
// DISASM-MATCH: = OpCompositeInsert %v4int %[0-9]+ %[0-9]+ 2
//
// v.z <<= 3 is a scalar shift of the one lane, put back with the insert a plain store uses.
kernel void compound_swizzle_one_lane_shift(device int4 *out [[buffer(0)]],
                                            uint i [[thread_position_in_grid]])
{
    int4 x = out[i];
    x.z <<= 3;
    out[i] = x;
}
