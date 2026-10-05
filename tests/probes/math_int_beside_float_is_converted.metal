// EXPECT: valid
// A builtin with no integer form takes the float argument's type when an
// integer scalar sits beside it, as Apple types pow(x, 2) as float; min, max
// and clamp reject the same mix as ambiguous.
// DISASM-MATCH: OpConvertSToF %float %int_2
// DISASM-MATCH: OpExtInst %float %[0-9]+ Pow %
// DISASM-MATCH: OpConvertSToF %float %int_0
// DISASM-MATCH: OpExtInst %float %[0-9]+ Step %
// DISASM-MATCH: OpConvertSToF %half %int_1
// DISASM-MATCH: OpExtInst %half %[0-9]+ FMix %
// DISASM-MATCH: OpExtInst %float %[0-9]+ Atan2 %
kernel void math_int_beside_float_is_converted(
    device const float* f [[buffer(0)]],
    device const half* h [[buffer(1)]],
    device const int* s [[buffer(2)]],
    device float* out [[buffer(3)]],
    device half* hout [[buffer(4)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = pow(f[i], 2) + step(0, f[i]) + atan2(s[i], f[i]);
    hout[i] = mix(h[i], h[i + 1u], 1);
}
