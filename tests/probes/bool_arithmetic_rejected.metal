// EXPECT: error an arithmetic, bitwise or comparison operator on a bool is not lowered yet
//
// Apple promotes the bool to int. mslc emitted OpIAdd with a bool result type,
// which spirv-val rejects, so it reports the operand instead.
kernel void bool_arithmetic_rejected(device int *out [[buffer(0)]], constant uint *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool flag = v[i] > 3u;
    out[i] = flag + 1;
}
