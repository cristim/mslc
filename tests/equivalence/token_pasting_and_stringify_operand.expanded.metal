kernel void equivalent(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint value_a = 5u;
    uint value_b = 6u;
    out[i] = value_a + value_b;
}
