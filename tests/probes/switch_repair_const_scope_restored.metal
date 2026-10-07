// EXPECT: valid
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+ 3 %[0-9]+
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+ 2 %[0-9]+
kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  const int label = 2;
  {
    const int label = 3;
    switch (gid) { case label: out[0] = 3; break; }
  }
  switch (gid) { case label: out[1] = 2; break; }
}
