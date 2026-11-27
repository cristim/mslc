// EXPECT: valid
// DEFINE: VALUE=42
// DISASM: OpConstant %int 42
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = VALUE; }
