// EXPECT: valid
// clamp picks FClamp, SClamp or UClamp from the operand's own type, the way
// min and max do, and broadcasts scalar bounds beside a vector.
// DISASM-MATCH: OpExtInst %float %[0-9]+ FClamp %
// DISASM-MATCH: OpExtInst %int %[0-9]+ SClamp %
// DISASM-MATCH: OpExtInst %uint %[0-9]+ UClamp %
// DISASM-MATCH: OpExtInst %v3float %[0-9]+ FClamp %
kernel void math_clamp(
    device const float* f [[buffer(0)]],
    device const int* s [[buffer(1)]],
    device const uint* u [[buffer(2)]],
    device const float3* v [[buffer(3)]],
    device float* fout [[buffer(4)]],
    device int* sout [[buffer(5)]],
    device uint* uout [[buffer(6)]],
    device float3* vout [[buffer(7)]],
    uint i [[thread_position_in_grid]])
{
    fout[i] = clamp(f[i], f[i + 1u], f[i + 2u]);
    sout[i] = clamp(s[i], s[i + 1u], s[i + 2u]);
    uout[i] = clamp(u[i], u[i + 1u], u[i + 2u]);
    vout[i] = clamp(v[i], 0.0f, 1.0f);
}
