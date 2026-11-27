// EXPECT: valid
// FLAG: --mask-grid-size
// The guard reads the thread position even when the kernel does not
// DISASM: BuiltIn GlobalInvocationId
// DISASM: OpUGreaterThanEqual
// REFLECT: "grid_size_spec_ids"
kernel void k(device uint* out [[buffer(0)]])
{ out[0] = 7; }
