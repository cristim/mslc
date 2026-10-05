// EXPECT: valid
// exp2 is GLSL.std.450 Exp2, on a float and on a half vector.
// DISASM-MATCH: OpExtInst %float %[0-9]+ Exp2 %
// DISASM-MATCH: OpExtInst %v4half %[0-9]+ Exp2 %
kernel void math_exp2(
    device const float* f [[buffer(0)]],
    device const half4* h [[buffer(1)]],
    device float* fout [[buffer(2)]],
    device half4* hout [[buffer(3)]],
    uint i [[thread_position_in_grid]])
{
    fout[i] = exp2(f[i]);
    hout[i] = exp2(h[i]);
}
