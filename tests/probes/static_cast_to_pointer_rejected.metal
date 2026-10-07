// EXPECT: error a cast to a qualified, pointer or reference type is not supported
//
// Apple: "static_cast from 'device uint *' to 'device float *' is not allowed".
kernel void static_cast_to_pointer_rejected(device float* out [[buffer(0)]], device uint* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    device float* p = static_cast<device float*>(in);
    out[i] = p[i];
}
