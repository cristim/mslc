// float + int is float + float(int). The float is on the left so that the two
// spellings emit their operands in the same order and number their ids alike.
kernel void k(device float *out [[buffer(0)]], device const int *in [[buffer(1)]],
              uint i [[thread_position_in_grid]])
{
    int x = in[i];
    out[i] = 1.5f + float(x);
}
