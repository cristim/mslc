// EXPECT: error assigning to a conditional expression is not supported
//
// Apple accepts `(c ? a : b) = x` when both arms are lvalues of one type, as C++
// does; mslc builds the conditional as a value only and refuses to assign to it.
kernel void ternary_assigned_to_rejected(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float a = 0.0f;
    float b = 0.0f;
    (in[0] > 0.0f ? a : b) = 1.0f;
    out[i] = a + b;
}
