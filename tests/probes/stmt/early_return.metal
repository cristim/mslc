// EXPECT: valid
kernel void early_return(device uint* out [[buffer(0)]],
                         uint i [[thread_position_in_grid]])
{
    if (i >= 4u) { return; }
    out[i] = i;
    return;
}
