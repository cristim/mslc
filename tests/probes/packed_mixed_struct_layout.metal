// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 0 Offset 0
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 4
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 16
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 3 Offset 20
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 4 Offset 36
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 5 Offset 48
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 6 Offset 64
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 7 Offset 70
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 8 Offset 71
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 9 Offset 74
// DISASM-MATCH: OpDecorate %_runtimearr__struct_[0-9]+ ArrayStride 80
//
// Offsets and size are Apple's, from static_assert on offsetof and sizeof.
struct Mixed
{
    float a;
    packed_float3 b;
    float c;
    packed_float4 d;
    packed_float2 e;
    float4 f;
    packed_half3 g;
    char h;
    packed_uchar3 i;
    packed_short2 j;
};

kernel void packed_mixed_struct_layout(device Mixed *out [[buffer(0)]],
                                       device const Mixed *in [[buffer(1)]])
{
    out[0].a = in[0].c;
    out[0].b = in[0].d.xyz;
    out[0].c = in[0].a;
    out[0].d = float4(in[0].b, 1.0);
    out[0].e = in[0].e;
    out[0].f = in[0].f;
    out[0].g = in[0].g;
    out[0].h = in[0].h;
    out[0].i = in[0].i;
    out[0].j = in[0].j;
}
