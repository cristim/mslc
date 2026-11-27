// EXPECT: error cannot store through "x"
//
// Apple: cannot assign to variable 'x' with const-qualified type.
kernel void step_const_local_rejected(device int *out [[buffer(0)]],
                                      uint i [[thread_position_in_grid]])
{
    const int x = 1;
    x++;
    out[i] = x;
}
