// EXPECT: valid
// DISASM: = OpConvertSToF %float
//
// static_cast<int> is the functional cast under an explicit spelling.
kernel void static_cast_int_to_float(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    int n = int(i);
    out[i] = static_cast<float>(n);
}
