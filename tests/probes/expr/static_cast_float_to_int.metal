// EXPECT: valid
// DISASM: = OpConvertFToS %int
kernel void static_cast_float_to_int(device int* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = static_cast<int>(in[i]);
}
