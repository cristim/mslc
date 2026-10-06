// EXPECT: error a cast to void is not supported
//
// Apple: "left operand to ? is void, but right operand is of type 'float'".
kernel void ternary_void_arm_rejected(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float r = in[0] > 0.0f ? (void)0 : 1.0f;
    out[i] = r;
}
