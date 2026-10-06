// int4 op= int4 and op= scalar are the plain operator.
kernel void vector_integer_operators(device int4 *out [[buffer(0)]], device const int4 *in [[buffer(1)]],
                                     uint i [[thread_position_in_grid]])
{
    int4 x = in[i];
    int4 y = in[i + 1u];
    x += y;
    x -= y;
    x *= y;
    x /= y;
    x %= y;
    x &= y;
    x |= y;
    x ^= y;
    x += 3;
    x &= 3;
    x %= 3;
    out[i] = x;
}
