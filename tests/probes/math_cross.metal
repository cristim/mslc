// EXPECT: valid
// cross is defined only for three-component vectors.
// DISASM-MATCH: OpExtInst %v3float %[0-9]+ Cross %
// DISASM-MATCH: OpExtInst %v3half %[0-9]+ Cross %
kernel void math_cross(
    device const float3* v [[buffer(0)]],
    device const half3* h [[buffer(1)]],
    device float3* vout [[buffer(2)]],
    device half3* hout [[buffer(3)]],
    uint i [[thread_position_in_grid]])
{
    vout[i] = cross(v[i], v[i + 1u]);
    hout[i] = cross(h[i], h[i + 1u]);
}
