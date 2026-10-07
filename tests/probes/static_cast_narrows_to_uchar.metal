// EXPECT: valid
// DISASM: = OpUConvert %uchar
//
// Narrowing truncates through OpUConvert.
kernel void static_cast_narrows_to_uchar(device uchar* out [[buffer(0)]], constant int* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = static_cast<uchar>(in[i]);
}
