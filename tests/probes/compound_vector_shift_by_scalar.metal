// EXPECT: valid
// DISASM-MATCH: = OpCompositeConstruct %v4int %int_2 %int_2 %int_2 %int_2
// DISASM-MATCH: = OpShiftLeftLogical %v4int %[0-9]+ %[0-9]+
//
// int4 v; v <<= 2 shifts every lane by the scalar, so the count is spread to four lanes first.
kernel void compound_vector_shift_by_scalar(device int4 *out [[buffer(0)]],
                                            uint i [[thread_position_in_grid]])
{
    int4 x = out[i];
    x <<= 2;
    out[i] = x;
}
