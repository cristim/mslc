// EXPECT: valid
// refract's eta is a scalar of the vectors' component type, so a float eta
// beside half vectors is converted rather than broadcast.
// DISASM-MATCH: OpExtInst %v3float %[0-9]+ Refract %[0-9]+ %[0-9]+ %[0-9]+[^ 0-9]
// DISASM-MATCH: OpExtInst %v4half %[0-9]+ Refract %[0-9]+ %[0-9]+ %[0-9]+[^ 0-9]
// DISASM-MATCH: OpFConvert %half
// DISASM-NO-MATCH: OpCompositeConstruct
kernel void math_refract(
    device const float3* v [[buffer(0)]],
    device const half4* h [[buffer(1)]],
    device const float* eta [[buffer(2)]],
    device float3* vout [[buffer(3)]],
    device half4* hout [[buffer(4)]],
    uint i [[thread_position_in_grid]])
{
    vout[i] = refract(v[i], v[i + 1u], eta[i]);
    hout[i] = refract(h[i], h[i + 1u], eta[i]);
}
