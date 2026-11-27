// EXPECT: valid
// DISASM-MATCH: OpDecorate %[0-9]+ Location 0
// DISASM-MATCH: OpDecorate %[0-9]+ Location 2
// DISASM-MATCH: OpDecorate %[0-9]+ Location 5
// DISASM-NOT: Location 1
// DISASM-NOT: Flat
// DISASM-MATCH: OpVariable %_ptr_Output_v4float Output
// DISASM-MATCH: OpVariable %_ptr_Output_uint Output
// DISASM-MATCH: OpVariable %_ptr_Output_v2float Output
//
// Each [[color(n)]] field is the Output at Location n, whatever its position in
// the struct. Apple takes the attachment index as written and allows gaps. An
// integer output is not interpolated, so it carries no Flat.
struct Out {
    uint hit [[color(5)]];
    float4 color [[color(0)]];
    half2 extra [[color(2)]];
};

fragment Out fragment_color_outputs_take_their_locations()
{
    Out o;
    o.hit = 7u;
    o.color = float4(1.0);
    o.extra = half2(0.5h);
    return o;
}
