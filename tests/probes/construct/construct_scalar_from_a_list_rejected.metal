// EXPECT: error float is built from one value, and this passes 2
//
// Apple reports "excess elements in scalar initializer".
kernel void construct_scalar_from_a_list_rejected(device float *out [[buffer(0)]], constant float *s [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = float(s[i], s[i]);
}
