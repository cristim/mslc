// EXPECT: valid
// fma is GLSL.std.450 Fma, whose operands are all the result type, so
// a scalar argument beside vectors is broadcast.
// DISASM-MATCH: OpExtInst %v3float %[0-9]+ Fma %
// DISASM-MATCH: OpExtInst %v4half %[0-9]+ Fma %
// DISASM-MATCH: OpCompositeConstruct %v3float
kernel void math_fma(
    device const float* f [[buffer(0)]],
    device const float3* v [[buffer(1)]],
    device const half4* h [[buffer(2)]],
    device float3* vout [[buffer(3)]],
    device half4* hout [[buffer(4)]],
    uint i [[thread_position_in_grid]])
{
    vout[i] = fma(v[i], v[i + 1u], f[i]);
    hout[i] = fma(h[i], h[i + 1u], h[i + 2u]);
}
