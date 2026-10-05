// EXPECT: valid
// tan is GLSL.std.450 Tan, on a float and on a half vector.
// DISASM-MATCH: OpExtInst %float %[0-9]+ Tan %
// DISASM-MATCH: OpExtInst %v4half %[0-9]+ Tan %
kernel void math_tan(
    device const float* f [[buffer(0)]],
    device const half4* h [[buffer(1)]],
    device float* fout [[buffer(2)]],
    device half4* hout [[buffer(3)]],
    uint i [[thread_position_in_grid]])
{
    fout[i] = tan(f[i]);
    hout[i] = tan(h[i]);
}
