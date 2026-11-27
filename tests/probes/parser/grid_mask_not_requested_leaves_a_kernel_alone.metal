// EXPECT: valid
// DISASM-NOT: SpecId 262144
// DISASM-NOT: OpUGreaterThanEqual
// REFLECT-NOT: grid_size_spec_ids
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i + 1; }
