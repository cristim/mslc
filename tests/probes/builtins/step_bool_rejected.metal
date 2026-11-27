// EXPECT: error ++ and -- do not apply to a bool
//
// Apple rejects incrementing a bool.
kernel void step_bool_rejected(device int *out [[buffer(0)]],
                               uint i [[thread_position_in_grid]])
{
    bool x = out[i] > 0;
    x++;
    out[i] = x;
}
