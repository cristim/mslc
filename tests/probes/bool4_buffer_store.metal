// EXPECT: valid
// DISASM-MATCH: OpDecorate %_runtimearr__arr_uchar_uint_4 ArrayStride 4
// DISASM-MATCH: OpSelect %v4uchar
//
// A bool vector is stored by selecting a byte vector and writing it lane by lane.
kernel void bool4_buffer_store(device bool4 *flags [[buffer(0)]], device const int4 *in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    flags[i] = in[i] > int4(2);
}
