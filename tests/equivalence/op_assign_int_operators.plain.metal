// x op= y is x = x op y for every operator, on int: the same module, not only a valid one.
kernel void int_operators(device int *out [[buffer(0)]], device const int *in [[buffer(1)]],
                          uint i [[thread_position_in_grid]])
{
    int x = in[i];
    int y = in[i + 1u];
    x = x + y;
    x = x - y;
    x = x * y;
    x = x / y;
    x = x % y;
    x = x & y;
    x = x | y;
    x = x ^ y;
    x = x << y;
    x = x >> y;
    out[i] = x;
}
