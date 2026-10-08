// EXPECT: error const_cast is not supported
//
// Apple: "const_cast to 'int', which is not a reference, pointer-to-object, or
// pointer-to-data-member".
kernel void const_cast_rejected(device float* out [[buffer(0)]], constant int* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    const int x = in[i];
    out[i] = float(const_cast<int>(x));
}
