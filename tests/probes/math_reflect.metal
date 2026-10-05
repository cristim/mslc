// EXPECT: valid
// reflect takes vectors and returns their type. A scalar normal is broadcast,
// as Apple's compiler converts a scalar to any vector.
// DISASM-MATCH: OpExtInst %v3float %[0-9]+ Reflect %
// DISASM-MATCH: OpExtInst %v4half %[0-9]+ Reflect %
// DISASM-MATCH: OpCompositeConstruct %v3float
kernel void math_reflect(
    device const float3* v [[buffer(0)]],
    device const half4* h [[buffer(1)]],
    device const float* f [[buffer(2)]],
    device float3* vout [[buffer(3)]],
    device half4* hout [[buffer(4)]],
    device float3* wout [[buffer(5)]],
    uint i [[thread_position_in_grid]])
{
    vout[i] = reflect(v[i], v[i + 1u]);
    hout[i] = reflect(h[i], h[i + 1u]);
    wout[i] = reflect(v[i], f[i]);
}
