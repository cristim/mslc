// EXPECT: error converting to or from bool is not lowered yet
// No convert opcode takes a bool, so this used to emit an OpBitcast or an
// OpUConvert that spirv-val rejected. Reporting it is better than that.
kernel void bool_conversion_rejected(device uint* out [[buffer(0)]],
                                     uint i [[thread_position_in_grid]])
{
    bool flag = i > 0u;
    out[i] = uint(flag);
}
