// EXPECT: valid
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+ 1 %[0-9]+
kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  const bool label = true;
  switch (bool(gid)) { case label: out[0] = 1; break; }
}
