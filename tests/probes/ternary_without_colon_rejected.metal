// EXPECT: error between the two values of a conditional
kernel void ternary_without_colon_rejected(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = in[0] > 0.0f ? 1.0f;
}
