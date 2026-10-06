// EXPECT: error a cast to a qualified, pointer or reference type is not supported
kernel void cast_to_a_pointer_rejected(device float* out [[buffer(0)]], device uint* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    device float* p = (device float*)in;
    out[i] = p[i];
}
