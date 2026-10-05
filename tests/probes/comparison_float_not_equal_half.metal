// EXPECT: valid
// DISASM-MATCH: = OpFUnordNotEqual %bool
// DISASM-NO-MATCH: = OpFOrdNotEqual %bool
//
// != on two %half operands is unordered too: NaN != x is true.
kernel void comparison_float_not_equal_half(device int *out [[buffer(0)]],
                                            device const half *in [[buffer(1)]],
                                            uint index [[thread_position_in_grid]])
{
    half a = in[index];
    half b = in[index + 1];

    if (a != b) { out[0] = 1; } else { out[0] = 0; }
}
