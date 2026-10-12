// EXPECT: valid
//
// Apple accepts static_cast<void>(x), as it accepts the C-style cast.
kernel void static_cast_to_void_rejected(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float x = in[0];
    static_cast<void>(x);
    out[i] = x;
}
