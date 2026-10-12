// EXPECT: valid
//
// Apple accepts `(void)x;`: the operand is evaluated and its value dropped.
kernel void cast_to_void_rejected(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float x = in[0];
    (void)x;
    out[i] = x;
}
