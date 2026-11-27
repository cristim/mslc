// EXPECT: valid
// DISASM-MATCH: = OpConvertSToF %float
// DISASM-MATCH: = OpFOrdLessThan %v3bool
// DISASM-MATCH: = OpFOrdGreaterThan %v2bool
// DISASM-NO-MATCH: = OpSLessThan
// DISASM-NO-MATCH: = OpSGreaterThan
//
// An int scalar beside a float vector is converted to float, and the float
// vector decides that the comparison is a float one on either side.
kernel void comparison_vector_int_scalar_float_vector(device int3 *out [[buffer(0)]],
                                                      constant int *s [[buffer(1)]],
                                                      constant float3 *f3 [[buffer(2)]],
                                                      constant float2 *f2 [[buffer(3)]],
                                                      uint i [[thread_position_in_grid]])
{
    int3 r = int3(s[i] < f3[i]);
    r.xy += int2(f2[i] > s[i]);
    out[i] = r;
}
