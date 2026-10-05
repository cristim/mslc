kernel void equivalent(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint first = 1u; uint second = 2u; uint third = 3u;
    out[i] = 0u + first + second + third;
}
