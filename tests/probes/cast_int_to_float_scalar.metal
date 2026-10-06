// EXPECT: valid
// DISASM: = OpConvertSToF %float
//
// `(float)n` is the functional cast `float(n)`.
kernel void cast_int_to_float_scalar(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    int n = int(i);
    out[i] = (float)n;
}
