// EXPECT: valid
// floor is GLSL.std.450 Floor, on a float and on a half vector.
// DISASM-MATCH: OpExtInst %float %[0-9]+ Floor %
// DISASM-MATCH: OpExtInst %v4half %[0-9]+ Floor %
kernel void math_floor(
    device const float* f [[buffer(0)]],
    device const half4* h [[buffer(1)]],
    device float* fout [[buffer(2)]],
    device half4* hout [[buffer(3)]],
    uint i [[thread_position_in_grid]])
{
    fout[i] = floor(f[i]);
    hout[i] = floor(h[i]);
}
