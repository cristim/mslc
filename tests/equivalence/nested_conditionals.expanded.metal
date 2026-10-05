kernel void equivalent(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint v = i;
    v = v * 2u;
    out[i] = v;
}
