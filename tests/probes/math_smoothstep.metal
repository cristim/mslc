// EXPECT: valid
// smoothstep is GLSL.std.450 SmoothStep, whose operands are all the result type, so
// a scalar argument beside vectors is broadcast.
// DISASM-MATCH: OpExtInst %v3float %[0-9]+ SmoothStep %
// DISASM-MATCH: OpExtInst %v4half %[0-9]+ SmoothStep %
// DISASM-MATCH: OpCompositeConstruct %v3float
// edge0, edge1, x in order.
// DISASM-MATCH: SmoothStep %float_0_25[_0-9]* %float_0_5[_0-9]* %[0-9]+
kernel void math_smoothstep(
    device const float* f [[buffer(0)]],
    device const float3* v [[buffer(1)]],
    device const half4* h [[buffer(2)]],
    device float3* vout [[buffer(3)]],
    device half4* hout [[buffer(4)]],
    device float* fout [[buffer(5)]],
    uint i [[thread_position_in_grid]])
{
    vout[i] = smoothstep(0.0f, f[i], v[i]);
    hout[i] = smoothstep(h[i], h[i + 1u], h[i + 2u]);
    fout[i] = smoothstep(0.25f, 0.5f, f[i]);
}
