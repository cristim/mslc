// EXPECT: valid
// log2 is GLSL.std.450 Log2, on a float and on a half vector.
// DISASM-MATCH: OpExtInst %float %[0-9]+ Log2 %
// DISASM-MATCH: OpExtInst %v4half %[0-9]+ Log2 %
kernel void math_log2(
    device const float* f [[buffer(0)]],
    device const half4* h [[buffer(1)]],
    device float* fout [[buffer(2)]],
    device half4* hout [[buffer(3)]],
    uint i [[thread_position_in_grid]])
{
    fout[i] = log2(f[i]);
    hout[i] = log2(h[i]);
}
