// EXPECT: error an arithmetic, bitwise or comparison operator on a bool is not lowered yet
//
// A bool operand on either side is reported, not typed as the other operand.
kernel void bool_and_int_rejected(device int *out [[buffer(0)]], constant uint *v [[buffer(1)]], uint k [[thread_position_in_grid]])
{
    bool flag = v[k] > 3u;
    int i = int(v[k]);
    float f = float(v[k]);
    out[k] = flag & i;
}
