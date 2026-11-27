// EXPECT: valid
// sin is GLSL.std.450 Sin, on a float and on a half vector.
// DISASM-MATCH: OpExtInst %float %[0-9]+ Sin %
// DISASM-MATCH: OpExtInst %v4half %[0-9]+ Sin %
kernel void math_sin(
    device const float* f [[buffer(0)]],
    device const half4* h [[buffer(1)]],
    device float* fout [[buffer(2)]],
    device half4* hout [[buffer(3)]],
    uint i [[thread_position_in_grid]])
{
    fout[i] = sin(f[i]);
    hout[i] = sin(h[i]);
}
