// EXPECT: valid
// DISASM-NO-MATCH: OpBitcast %_struct
// DISASM-MATCH: OpCompositeExtract %float %[0-9]+ 0[^=%a-zA-Z]+%[0-9]+ = OpCompositeExtract %float %[0-9]+ 1[^=%a-zA-Z]+%[0-9]+ = OpCompositeExtract %float %[0-9]+ 2[^=%a-zA-Z]+%[0-9]+ = OpCompositeConstruct %v3float
// DISASM-MATCH: OpCompositeExtract %float %[0-9]+ 0[^=%a-zA-Z]+%[0-9]+ = OpCompositeExtract %float %[0-9]+ 1[^=%a-zA-Z]+%[0-9]+ = OpCompositeExtract %float %[0-9]+ 2[^=%a-zA-Z]+%[0-9]+ = OpCompositeConstruct %_arr_float_uint_3
//
// A packed_float3 is an array of three floats in the buffer's struct and a
// vector in a local's, so the round trip buffer to local to buffer turns the
// member both ways. Read back on lavapipe and on an Apple GPU the stored values
// match; the padding the host never reads is not written. The two matches pin
// the lane order of each direction, which a pure round trip cannot, because a
// reversal in both directions cancels.
struct P {
    float a;
    packed_float3 p;
    float b;
};

kernel void struct_copy_packed_member_round_trip(device P* out [[buffer(0)]],
                                                 const device P* in [[buffer(1)]])
{
    P z = in[1];
    z.b = z.p.x + 100.0;
    out[0] = z;
    P y = in[0];
    out[1] = y;
}
