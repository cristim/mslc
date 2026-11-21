// EXPECT: valid
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+ 3 %
// "case ::n:" is a constant expression though a runtime local n hides the name.
#include <metal_stdlib>
using namespace metal;
constant int n = 3;
kernel void kern(device int* out [[buffer(0)]], uint tid [[thread_position_in_grid]]) { int r = 0; { int n = int(tid); switch (int(tid)) { case ::n: r = 1; break; default: r = 2; } } out[0] = r; }
