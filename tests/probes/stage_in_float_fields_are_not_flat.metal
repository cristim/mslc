// EXPECT: valid
// DISASM-MATCH: OpDecorate %[0-9]+ Location 2
// DISASM-NOT: Flat
// DISASM: = OpVariable %_ptr_Input_v2float Input
//
// Only an integer fragment input is Flat. A float, a half (carried as a float)
// or a float vector is interpolated, and a Flat on one of them is spirv-val
// valid and silently stops the interpolation, so none may appear here.
struct In {
    float4 position [[position]];
    float a;
    half b;
    float2 c;
};

fragment float4 stage_in_float_fields_are_not_flat(In in [[stage_in]])
{
    float2 c = in.c;
    return float4(in.a) + float4(float(in.b));
}
