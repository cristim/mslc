// EXPECT: valid
// RUN: grid=64 local=8 cases=3
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
                                       device const Mixed *in [[buffer(1)]],
                                       uint i [[thread_position_in_grid]])
{
    out[i].a = in[i].c;
    out[i].b = in[i].d.xyz;
    out[i].c = in[i].a;
    out[i].d = float4(in[i].b, 1.0);
    out[i].e = in[i].e;
    out[i].f = in[i].f;
    out[i].g = in[i].g;
    out[i].h = in[i].h;
    out[i].i = in[i].i;
    out[i].j = in[i].j;
}
