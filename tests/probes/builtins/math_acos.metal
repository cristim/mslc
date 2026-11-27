// EXPECT: valid
// acos is GLSL.std.450 Acos, on a float and on a half vector.
// DISASM-MATCH: OpExtInst %float %[0-9]+ Acos %
// DISASM-MATCH: OpExtInst %v4half %[0-9]+ Acos %
kernel void math_acos(
    device const float* f [[buffer(0)]],
    device const half4* h [[buffer(1)]],
    device float* fout [[buffer(2)]],
    device half4* hout [[buffer(3)]],
    uint i [[thread_position_in_grid]])
{
    fout[i] = acos(f[i]);
    hout[i] = acos(h[i]);
}
