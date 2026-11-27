// EXPECT: valid
// DISASM-MATCH: OpEntryPoint Fragment %[0-9]+ "stage_in_fragment_struct" %gl_FragCoord %[0-9]+ %[0-9]+ %[0-9]+ %[0-9]+
// DISASM: %gl_FragCoord = OpVariable %_ptr_Input_v4float Input
// DISASM-ORDER: OpDecorate %gl_FragCoord BuiltIn FragCoord
// DISASM-ORDER: Location 0
// DISASM-ORDER: Location 1
// DISASM-ORDER: Flat
// DISASM-ORDER: Flat
// DISASM-ORDER: Location 2
// DISASM-MATCH: OpDecorate %[0-9]+ Location 0
// DISASM-MATCH: OpDecorate %[0-9]+ Location 1
// DISASM-MATCH: OpDecorate %[0-9]+ Location 2
// DISASM-MATCH: OpDecorate %[0-9]+ Flat
// DISASM: = OpVariable %_ptr_Input_int Input
// DISASM: = OpVariable %_ptr_Input_float Input
// DISASM-NOT: OpTypePointer Input %half
// DISASM: OpFConvert %half
//
// A fragment function's [[stage_in]] struct is one Input per field: the
// [[position]] field is FragCoord, and the rest take Locations 0, 1, 2 in
// declaration order. The int field alone is Flat, since an integer is not
// interpolated and Vulkan rejects an unflat integer fragment input. The half
// field crosses as a float and is narrowed when the struct is filled in.
struct In {
    float4 position [[position]];
    float4 color;
    int index;
    half weight;
};

fragment float4 stage_in_fragment_struct(In in [[stage_in]])
{
    return in.color * float(in.index) * float(in.weight);
}
