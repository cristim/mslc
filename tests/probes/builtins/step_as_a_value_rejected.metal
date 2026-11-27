// EXPECT: error ++ and -- are lowered only as a statement or a for-loop increment
//
// The value of x++ is not lowered.
kernel void step_as_a_value_rejected(device int *out [[buffer(0)]],
                                     uint i [[thread_position_in_grid]])
{
    int x = out[i];
    out[i] = x++;
}
