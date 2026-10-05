// EXPECT: error is lowered only for numeric scalars, vectors and matrices
//
// Apple accepts b &= c; the conversion of a bool to an integer is not lowered.
kernel void compound_bool_target_rejected(device bool *out [[buffer(0)]], device const bool *b [[buffer(1)]],
                                          uint i [[thread_position_in_grid]])
{
    bool x = out[i];
    x &= b[i];
    out[i] = x;
}
