// EXPECT: valid
// DISASM-MATCH: OpStore %[0-9]+ %[0-9]+ Aligned 4
// DISASM-MATCH: OpCompositeConstruct %_arr_uchar_uint_3
//
// Apple writes the three lanes of a bool3 and leaves the fourth byte alone,
// so the stored value is three bytes.
kernel void bool3_store_writes_three_bytes(device bool3 *flags [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    flags[i] = bool3(true, false, true);
}
