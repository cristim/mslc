// EXPECT: valid
// A change of signedness at the same width reinterprets the bits, which is what
// OpBitcast does, and which the spec permits only at equal width. Two's
// complement makes it value preserving here, so no convert opcode is needed and
// none is legal. OpUConvert and OpSConvert are for a change of width and
// spirv-val rejects them at this width.
// DISASM: OpBitcast
// DISASM-NOT: OpUConvert
// DISASM-NOT: OpSConvert
kernel void reinterpret_int(device const uint* w [[buffer(0)]],
                            device int* out [[buffer(1)]],
                            uint i [[thread_position_in_grid]])
{ out[i] = w[i]; }
