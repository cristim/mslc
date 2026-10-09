// EXPECT: valid
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+ 1 %[0-9]+ 2 %[0-9]+
constant int label = 1;
kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  switch (gid) {
    case label: break;
    case 2: int label; label = 5; out[0] = label; break;
  }
}
