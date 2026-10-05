// EXPECT: valid
// normalize takes vectors and returns their type.
// DISASM-MATCH: OpExtInst %v3float %[0-9]+ Normalize %
// DISASM-MATCH: OpExtInst %v4half %[0-9]+ Normalize %
kernel void math_normalize(
    device const float3* v [[buffer(0)]],
    device const half4* h [[buffer(1)]],
    device float3* vout [[buffer(2)]],
    device half4* hout [[buffer(3)]],
    uint i [[thread_position_in_grid]])
{
    vout[i] = normalize(v[i]);
    hout[i] = normalize(h[i]);
}
