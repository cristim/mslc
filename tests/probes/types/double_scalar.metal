// EXPECT: valid
// A 64-bit float needs Float64. The scalar table asked for Int64, which is
// the 64-bit integer capability, so double never validated.
// DISASM: OpCapability Float64
kernel void double_scalar(device const double* in [[buffer(0)]],
                          device double* out [[buffer(1)]],
                          uint i [[thread_position_in_grid]])
{ out[i] = in[i]; }
