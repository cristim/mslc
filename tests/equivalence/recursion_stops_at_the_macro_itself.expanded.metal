kernel void equivalent(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint value = i + 1u; uint a = 2u; uint b = 3u;
    out[i] = value + a + b;
}
