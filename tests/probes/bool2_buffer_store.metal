// EXPECT: valid
// DISASM-MATCH: OpCompositeExtract %bool %[0-9]+ 0
// DISASM-MATCH: OpCompositeExtract %bool %[0-9]+ 1
// DISASM-MATCH: OpSelect %uchar %[0-9]+ %uchar_1[_0-9]* %uchar_0[_0-9]*
// DISASM-NO-MATCH: OpSelect %uchar %[0-9]+ %uchar_0
// DISASM-ASCENDING: OpCompositeConstruct %_arr_uchar_uint_2
//
// Each lane is selected to 1 for true and 0 for false, in lane order.
kernel void bool2_buffer_store(device bool2 *flags [[buffer(0)]], device const int2 *in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    flags[i] = in[i] > int2(2);
}
