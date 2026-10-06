// EXPECT: valid
// DISASM-MATCH: = OpCompositeExtract %float %[0-9]+ 1
// DISASM-NO-MATCH: = OpCompositeExtract %float %[0-9]+ 0
//
// true is 1, so v[true] is lane 1.
kernel void vector_element_bool_literal_is_lane_one(device float *out [[buffer(0)]],
                                                    device const float4 *in [[buffer(1)]])
{
    float4 v = in[0];
    out[0] = v[true];
}
