// x op= y is x = x op y for the float operators.
kernel void float_operators(device float *out [[buffer(0)]], device const float *in [[buffer(1)]],
                            uint i [[thread_position_in_grid]])
{
    float x = in[i];
    float y = in[i + 1u];
    x = x + y;
    x = x - y;
    x = x * y;
    x = x / y;
    out[i] = x;
}
