// EXPECT: error a unary operator on a bool is not lowered yet
//
// -flag is an int in Apple's compiler. mslc emitted OpSNegate on a bool.
kernel void bool_negate_rejected(device int *out [[buffer(0)]], constant uint *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool flag = v[i] > 3u;
    out[i] = -flag;
}
