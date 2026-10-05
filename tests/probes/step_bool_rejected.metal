// EXPECT: error is lowered only for numeric scalars, vectors and matrices
//
// Apple rejects incrementing a bool.
kernel void step_bool_rejected(device bool *out [[buffer(0)]],
                               uint i [[thread_position_in_grid]])
{
    bool x = out[i];
    x++;
    out[i] = x;
}
