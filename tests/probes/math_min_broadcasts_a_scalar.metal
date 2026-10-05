// EXPECT: valid
// A scalar beside a vector is broadcast to it, on either side, as Metal does.
// DISASM-MATCH: OpExtInst %v3float %[0-9]+ FMin %
// DISASM-MATCH: OpExtInst %v3float %[0-9]+ FMax %
// DISASM-MATCH: OpCompositeConstruct %v3float
kernel void math_min_broadcasts_a_scalar(
    device const float* f [[buffer(0)]],
    device const float3* v [[buffer(1)]],
    device float3* vout [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    vout[i] = min(v[i], f[i]) + max(f[i], v[i]);
}
