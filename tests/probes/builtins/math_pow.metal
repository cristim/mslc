// EXPECT: valid
// pow is GLSL.std.450 Pow. A scalar beside a vector is broadcast, as
// Metal's own overloads do, since GLSL.std.450 wants every operand of the
// result type.
// DISASM-MATCH: OpExtInst %float %[0-9]+ Pow %
// DISASM-MATCH: OpExtInst %v4half %[0-9]+ Pow %
// DISASM-MATCH: OpExtInst %v3float %[0-9]+ Pow %
// DISASM-MATCH: OpCompositeConstruct %v3float
kernel void math_pow(
    device const float* f [[buffer(0)]],
    device const half4* h [[buffer(1)]],
    device const float3* v [[buffer(2)]],
    device float* fout [[buffer(3)]],
    device half4* hout [[buffer(4)]],
    device float3* vout [[buffer(5)]],
    uint i [[thread_position_in_grid]])
{
    fout[i] = pow(f[i], f[i + 1u]);
    hout[i] = pow(h[i], h[i + 1u]);
    vout[i] = pow(v[i], f[i]);
}
