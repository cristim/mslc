// EXPECT: valid
// DISASM-MATCH: OpConstantComposite %v4float %float_1 %float_2 %float_3 %float_4
// DISASM-MATCH: OpConstantComposite %v4float %float_5 %float_5[_0-9]* %float_5[_0-9]* %float_5[_0-9]*
//
// A file-scope float4 constant built by its own constructor: component by component, and the
// one-argument splat. Apple takes both.
constant float4 kList = float4(1, 2, 3, 4);
constant float4 kSplat = float4(5);
constant half2 kHalf = half2(0.5h, 1.5h);

kernel void file_scope_constant_vector_constructor(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = kList.z + kSplat.w + float(kHalf.y);
}
