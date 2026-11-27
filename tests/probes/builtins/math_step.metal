// EXPECT: valid
// step(edge, x) is GLSL.std.450 Step in the same argument order; a scalar
// edge beside a vector is broadcast.
// DISASM-MATCH: OpExtInst %v3float %[0-9]+ Step %
// DISASM-MATCH: OpExtInst %half %[0-9]+ Step %
// DISASM-MATCH: OpCompositeConstruct %v3float
// The edge comes first: a swapped pair would put %float_0_5 last.
// DISASM-MATCH: OpExtInst %float %[0-9]+ Step %float_0_5[_0-9]* %[0-9]+
kernel void math_step(
    device const float* f [[buffer(0)]],
    device const float3* v [[buffer(1)]],
    device const half* h [[buffer(2)]],
    device float3* vout [[buffer(3)]],
    device half* hout [[buffer(4)]],
    device float* fout [[buffer(5)]],
    uint i [[thread_position_in_grid]])
{
    vout[i] = step(f[i], v[i]);
    hout[i] = step(h[i], h[i + 1u]);
    fout[i] = step(0.5f, f[i]);
}
