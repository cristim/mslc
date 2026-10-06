// EXPECT: error this expression cannot be used as a value
//
// Plain assignment used as a value is rejected the same way.
kernel void compound_assignment_as_a_value_rejected(device int *out [[buffer(0)]],
                                                    uint i [[thread_position_in_grid]])
{
    int x = out[i];
    out[i] = (x += 1);
}
