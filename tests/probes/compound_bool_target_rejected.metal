// EXPECT: error is lowered only for numeric scalars, vectors and matrices
//
// Apple accepts b &= c; a compound assignment on a bool is not lowered.
kernel void compound_bool_target_rejected(device int *out [[buffer(0)]],
                                          uint i [[thread_position_in_grid]])
{
    bool x = out[i] > 0;
    x &= 1;
    out[i] = x;
}
