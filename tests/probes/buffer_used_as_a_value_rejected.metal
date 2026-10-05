// EXPECT: error the buffer "c" is used as a value, which is not lowered yet
//
// A buffer is reached through the binding-0 address block, so its name has no
// id of its own; this used to load from id 0 and hand back an invalid module.
kernel void buffer_used_as_a_value_rejected(device float4 *out [[buffer(0)]],
                                            constant float4 &c [[buffer(1)]],
                                            uint i [[thread_position_in_grid]])
{
    out[i] = c;
}
