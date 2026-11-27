// EXPECT: valid
// OpBitcast is a reinterpretation and the spec allows it only between operands
// of equal width, so a change of width needs OpUConvert (zero extend or
// truncate) or OpSConvert (sign extend or truncate). The result type's
// signedness picks which one, and spirv-val requires them to agree.
// DISASM: OpSConvert
// DISASM: OpUConvert
kernel void widen_int(device const short* s [[buffer(0)]],
                      device const ushort* u [[buffer(1)]],
                      device int* out [[buffer(2)]],
                      device uint* uout [[buffer(3)]],
                      uint i [[thread_position_in_grid]])
{
    out[i] = s[i];
    uout[i] = u[i];
}
