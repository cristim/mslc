// EXPECT: valid
// DISASM-MATCH: = OpSLessThan %v3bool
// DISASM-MATCH: = OpSLessThan %v4bool
// DISASM-NO-MATCH: = OpSLessThan %bool
//
// With the scalar on the left the result width is the vector's, at 3 and 4
// as well as 2.
kernel void comparison_vector_scalar_left_wide(device int4 *out [[buffer(0)]],
                                               constant int *s [[buffer(1)]],
                                               constant int3 *a [[buffer(2)]],
                                               constant int4 *b [[buffer(3)]],
                                               uint i [[thread_position_in_grid]])
{
    int4 r = int4(s[i] < b[i]);
    r.xyz += int3(s[i] < a[i]);
    out[i] = r;
}
