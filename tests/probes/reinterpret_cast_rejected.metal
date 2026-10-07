// EXPECT: error reinterpret_cast is not supported
//
// Apple: "reinterpret_cast from 'int' to 'float' is not allowed".
kernel void reinterpret_cast_rejected(device float* out [[buffer(0)]], constant int* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = reinterpret_cast<float>(in[i]);
}
