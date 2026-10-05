// EXPECT: valid
// distance takes vectors and returns their component type.
// DISASM-MATCH: OpExtInst %float %[0-9]+ Distance %
// DISASM-MATCH: OpExtInst %half %[0-9]+ Distance %
kernel void math_distance(
    device const float3* v [[buffer(0)]],
    device const half4* h [[buffer(1)]],
    device float* fout [[buffer(2)]],
    device half* hout [[buffer(3)]],
    uint i [[thread_position_in_grid]])
{
    fout[i] = distance(v[i], v[i + 1u]);
    hout[i] = distance(h[i], h[i + 1u]);
}
