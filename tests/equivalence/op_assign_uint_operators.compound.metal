// x op= y is x = x op y for every operator, on uint: the same module, not only a valid one.
kernel void uint_operators(device uint *out [[buffer(0)]], device const uint *in [[buffer(1)]],
                           uint i [[thread_position_in_grid]])
{
    uint x = in[i];
    uint y = in[i + 1u];
    x += y;
    x -= y;
    x *= y;
    x /= y;
    x %= y;
    x &= y;
    x |= y;
    x ^= y;
    x <<= y;
    x >>= y;
    out[i] = x;
}
