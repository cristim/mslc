// A float target with an int operand is the same as the plain operator, which converts the int.
kernel void float_by_int_converts_the_operand(device float *out [[buffer(0)]], device const int *in [[buffer(1)]],
                                              uint i [[thread_position_in_grid]])
{
    float x = out[i];
    int y = in[i];
    x = x + y;
    x = x - y;
    x = x * y;
    x = x / y;
    out[i] = x;
}
