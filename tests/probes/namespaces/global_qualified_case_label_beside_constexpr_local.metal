// EXPECT: valid
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+ 3 %
// "case ::n:" is the file-scope n (3), though a constexpr local n (7) hides it. Apple: label 3.
#include <metal_stdlib>
using namespace metal;
constant int n = 3;
kernel void kern(device int* out [[buffer(0)]], uint tid [[thread_position_in_grid]]) { int r = 0; { constexpr int n = 7; switch (int(tid) + 3) { case ::n: r = 1; break; default: r = 2; } } out[0] = r; }
