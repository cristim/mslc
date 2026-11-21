// EXPECT: valid
// DISASM-MATCH: OpConstantComposite %v4float %float_1 %float_1[_0-9]* %float_1[_0-9]* %float_1[_0-9]*
// DISASM-MATCH: OpConstantComposite %v4float %float_2 %float_2[_0-9]* %float_2[_0-9]* %float_2[_0-9]*
//
// Constructor elements in a file-scope constant array, beside the braced-row form.
constant float4 c[2] = { float4(1), float4(2) };

kernel void file_scope_constant_array_constructor_elements(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = c[i & 1].x;
}
