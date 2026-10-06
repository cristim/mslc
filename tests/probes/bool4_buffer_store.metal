// EXPECT: valid
// DISASM-MATCH: OpDecorate %_runtimearr__arr_uchar_uint_4 ArrayStride 4
// DISASM-MATCH: OpCompositeExtract %bool %[0-9]+ 0
// DISASM-MATCH: OpCompositeExtract %bool %[0-9]+ 1
// DISASM-MATCH: OpCompositeExtract %bool %[0-9]+ 2
// DISASM-MATCH: OpCompositeExtract %bool %[0-9]+ 3
// DISASM-MATCH: OpSelect %uchar %[0-9]+ %uchar_1[_0-9]* %uchar_0[_0-9]*
// DISASM-NO-MATCH: OpSelect %uchar %[0-9]+ %uchar_0
// DISASM-ASCENDING: OpCompositeConstruct %_arr_uchar_uint_4
//
// A bool vector is stored lane by lane: each lane is selected to 1 for true
// and 0 for false, and the bytes are written as an array.
kernel void bool4_buffer_store(device bool4 *flags [[buffer(0)]], device const int4 *in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    flags[i] = in[i] > int4(2);
}
