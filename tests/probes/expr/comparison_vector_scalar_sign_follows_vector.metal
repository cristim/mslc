// EXPECT: valid
// DISASM: = OpSLessThan %v2bool
// DISASM: = OpULessThan %v3bool
// DISASM: = OpSLessThan %v4bool
// DISASM: = OpULessThan %v2bool
// DISASM-NO-MATCH: = OpULessThan %v4bool
// DISASM-NO-MATCH: = OpSLessThan %v3bool
//
// A scalar beside a vector is converted to the vector's component type, so the
// vector decides the signedness whichever side the scalar is on: uint < int2
// and int4 < uint are signed, int < uint3 and uint2 < int are unsigned.
kernel void comparison_vector_scalar_sign_follows_vector(device int4 *out [[buffer(0)]],
                                                         constant uint *us [[buffer(1)]],
                                                         constant int *is [[buffer(2)]],
                                                         constant int2 *i2 [[buffer(3)]],
                                                         constant uint3 *u3 [[buffer(4)]],
                                                         constant int4 *i4 [[buffer(5)]],
                                                         constant uint2 *u2 [[buffer(6)]],
                                                         uint i [[thread_position_in_grid]])
{
    int4 r = int4(0);
    r.xy = int2(us[i] < i2[i]);
    r.xyz += int3(is[i] < u3[i]);
    r += int4(i4[i] < us[i]);
    r.xy += int2(u2[i] < is[i]);
    out[i] = r;
}
