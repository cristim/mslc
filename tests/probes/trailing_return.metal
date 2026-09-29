// EXPECT: valid
// An explicit return ends the entry block; no second OpReturn may follow it.
kernel void trailing_return(device uint* out [[buffer(0)]],
                            uint i [[thread_position_in_grid]])
{
    out[i] = i;
    return;
}
