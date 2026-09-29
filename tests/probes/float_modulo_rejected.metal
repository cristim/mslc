// EXPECT: error operator % needs integer operands
// Metal rejects % on float too; it used to lower to OpFMod, which floors.
kernel void float_modulo_rejected(device float* out [[buffer(0)]],
                                  uint i [[thread_position_in_grid]])
{ out[i] = out[i] % 2.0; }
