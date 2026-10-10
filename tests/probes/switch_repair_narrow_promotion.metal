// EXPECT: valid
// DISASM: OpUConvert %uint
// DISASM: OpBitcast %int
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+ -1 %[0-9]+ 300 %[0-9]+
kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  uchar s = uchar(gid);
  switch (s) {
    case -1: out[gid] = 100; break;
    case 300: out[gid] = 200; break;
    default: out[gid] = 3; break;
  }
}
