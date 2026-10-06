// EXPECT: valid
// DISASM-NO-MATCH: OpBitcast %_struct
//
// A packed_float3 is an array of three floats in the buffer's struct and a
// vector in a local's, so the round trip buffer to local to buffer turns the
// member both ways. Read back on lavapipe and on an Apple GPU the stored values
// match; the padding the host never reads is not written.
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
