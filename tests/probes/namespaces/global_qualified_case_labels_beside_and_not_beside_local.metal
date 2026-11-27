// EXPECT: valid
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+ 4 %[0-9]+ 8 %
// "case ::n + 1" (4) and "case n + 1" (8, the local) are two labels, not a duplicate.
#include <metal_stdlib>
using namespace metal;
constant int n = 3;
kernel void kern(device int* out [[buffer(0)]], uint tid [[thread_position_in_grid]]) { int r = 0; { constexpr int n = 7; switch (int(tid) + 8) { case ::n + 1: r = 1; break; case n + 1: r = 5; break; default: r = 2; } } out[0] = r; }
