// EXPECT: valid
// dot is the core OpDot, not a GLSL.std.450 instruction, and returns the
// component type.
// DISASM-MATCH: OpDot %float %
// DISASM-MATCH: OpDot %half %
// DISASM-NOT: OpExtInst
// DISASM-NOT: GLSL.std.450
kernel void math_dot(
    device const float4* v [[buffer(0)]],
    device const half3* h [[buffer(1)]],
    device float* fout [[buffer(2)]],
    device half* hout [[buffer(3)]],
    uint i [[thread_position_in_grid]])
{
    fout[i] = dot(v[i], v[i + 1u]);
    hout[i] = dot(h[i], h[i + 1u]);
}
