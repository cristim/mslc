// EXPECT: valid
// DISASM-ORDER: Location 0
// DISASM-ORDER: OpDecorate %gl_Position BuiltIn Position
// DISASM-ORDER: OpDecorate %gl_Position BuiltIn Position
// DISASM-ORDER: Location 1
// DISASM-ORDER: Location 1
// DISASM-ORDER: Location 2
// DISASM-MATCH: OpDecorate %[0-9]+ Location 0
// DISASM-MATCH: OpDecorate %[0-9]+ Location 1
// DISASM-MATCH: OpDecorate %[0-9]+ Location 2
// DISASM-NO-MATCH: Location 3
// DISASM-ORDER: = OpVariable %_ptr_Output_v4float Output
// DISASM-ORDER: %gl_Position = OpVariable %_ptr_Output_v4float Output
// DISASM-ORDER: %gl_Position = OpVariable %_ptr_Output_v4float Output
// DISASM-ORDER: = OpVariable %_ptr_Output_v2float Output
// DISASM-ORDER: = OpVariable %_ptr_Output_v2float Output
// DISASM-ORDER: = OpVariable %_ptr_Output_float Output
// DISASM-MATCH: OpEntryPoint Vertex %[0-9]+ "stage_out_locations_follow_declaration_order" %gl_VertexIndex %[0-9]+ %gl_Position %[0-9]+ %[0-9]+
// DISASM-MATCH: OpStore %gl_Position %[0-9]+
// DISASM-NOT: OpReturnValue
//
// A vertex function's returned struct becomes one Output per field. The
// [[position]] field is the BuiltIn Position wherever it sits, and every other
// field takes the next Location in declaration order, skipping the position, as
// Iridium numbers them: "a" is 0, "b" is 1, "c" is 2. The fragment side numbers
// the same way, which is why a vertex output and a fragment input meet.
struct Out {
    float4 a;
    float4 p [[position]];
    float2 b;
    float c;
};

vertex Out stage_out_locations_follow_declaration_order(uint vid [[vertex_id]])
{
    Out o;
    o.p = float4(1.0);
    o.a = float4(2.0);
    o.b = float2(3.0);
    o.c = 4.0;
    return o;
}
