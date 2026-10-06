// EXPECT: valid
// DISASM-MATCH: = OpFOrdLessThan %v4bool
// DISASM-MATCH: = OpFOrdGreaterThanEqual %v4bool
// DISASM-MATCH: = OpFOrdEqual %v4bool
//
// float vectors use the ordered opcodes, so a NaN lane compares false.
kernel void comparison_float_vector_ordered(device int2 *out [[buffer(0)]],
    constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    int4 r = int4(a[i] < b[i]) + int4(a[i] >= b[i]) + int4(a[i] == b[i]);
    out[i] = int2(r.x, r.w);
}
