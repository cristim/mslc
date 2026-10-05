// EXPECT: valid
// abs picks FAbs or SAbs from the operand's own type.
// DISASM-MATCH: OpExtInst %float %[0-9]+ FAbs %
// DISASM-MATCH: OpExtInst %v4half %[0-9]+ FAbs %
// DISASM-MATCH: OpExtInst %int %[0-9]+ SAbs %
kernel void math_abs(
    device const float* f [[buffer(0)]],
    device const half4* h [[buffer(1)]],
    device const int* s [[buffer(2)]],
    device float* fout [[buffer(3)]],
    device half4* hout [[buffer(4)]],
    device int* sout [[buffer(5)]],
    uint i [[thread_position_in_grid]])
{
    fout[i] = abs(f[i]);
    hout[i] = abs(h[i]);
    sout[i] = abs(s[i]);
}
