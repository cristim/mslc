// EXPECT: valid
// saturate is FClamp(x, 0, 1) with 0 and 1 of the operand's own type: two
// float constants for a float, and composites of half constants for a half4,
// where 1.0 is 0x3C00 and not the float's bits.
// DISASM-MATCH: OpExtInst %float %[0-9]+ FClamp %[0-9]+ %float_0 %float_1[^_0-9a-zA-Z]
// DISASM-MATCH: OpConstantComposite %v4half %half_0x1p_0 %half_0x1p_0 %half_0x1p_0 %half_0x1p_0[^_0-9a-zA-Z]
// DISASM-MATCH: OpConstantComposite %v4half %half_0x0p_0 %half_0x0p_0 %half_0x0p_0 %half_0x0p_0[^_0-9a-zA-Z]
// DISASM-MATCH: OpExtInst %v4half %[0-9]+ FClamp %
kernel void math_saturate(
    device const float* f [[buffer(0)]],
    device const half4* h [[buffer(1)]],
    device float* fout [[buffer(2)]],
    device half4* hout [[buffer(3)]],
    uint i [[thread_position_in_grid]])
{
    fout[i] = saturate(f[i]);
    hout[i] = saturate(h[i]);
}
