// EXPECT: valid
// The execution mode is LocalSize, which the emitter used to write as the bare
// number 17 while a comment claimed it came from the generated table. The table
// had no ExecutionMode category at all, so nothing could check the number.
// The trailing space and the operands keep the needle from matching
// LocalSizeId or LocalSizeHint as a substring, though in practice spirv-val
// rejects both of those before the disassembly is even looked at.
// DISASM: LocalSize 1 1 1
kernel void local_size_by_name(device const uint* in [[buffer(0)]],
                              device uint* out [[buffer(1)]],
                              uint i [[thread_position_in_grid]])
{ out[i] = in[i]; }
