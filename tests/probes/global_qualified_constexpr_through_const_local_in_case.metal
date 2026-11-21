// EXPECT: valid
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+ 4 %
// The same with const locals.
#include <metal_stdlib>
using namespace metal;
constant int n = 3;
kernel void kern(device int* out [[buffer(0)]], uint tid [[thread_position_in_grid]]) { int r = 0; { const int n = 7; const int m = ::n + 1; switch (int(tid) + 4) { case m: r = 1; break; default: r = 2; } } out[0] = r; }
