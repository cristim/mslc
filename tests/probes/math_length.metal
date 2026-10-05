// EXPECT: valid
// length takes vectors and returns their component type.
// DISASM-MATCH: OpExtInst %float %[0-9]+ Length %
// DISASM-MATCH: OpExtInst %half %[0-9]+ Length %
kernel void math_length(
    device const float3* v [[buffer(0)]],
    device const half4* h [[buffer(1)]],
    device float* fout [[buffer(2)]],
    device half* hout [[buffer(3)]],
    uint i [[thread_position_in_grid]])
{
    fout[i] = length(v[i]);
    hout[i] = length(h[i]);
}
