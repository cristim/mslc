// EXPECT: error cannot store through "in"
//
// Apple: cannot assign to return value because function 'operator[]' returns a const value.
kernel void compound_constant_buffer_rejected(device int *out [[buffer(0)]], constant int *in [[buffer(1)]],
                                              uint i [[thread_position_in_grid]])
{
    in[i] += 1;
    out[i] = in[i];
}
