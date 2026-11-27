// EXPECT: valid
// FLAG: --mask-grid-size
// A kernel skips the threads outside the grid size that three specialization constants give, which default to
// the largest value so that a dispatch that sets none runs every thread
// DISASM: OpSpecConstant %uint 4294967295
// DISASM: SpecId 262144
// DISASM: SpecId 262146
// DISASM: OpUGreaterThanEqual
// DISASM: OpLogicalOr
// DISASM: OpSelectionMerge
// REFLECT: "grid_size_spec_ids": [262144, 262145, 262146]
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i + 1; }
