// EXPECT: valid
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+ 4 %
// constexpr m = ::n + 1 is 4 (the file-scope n), so "case m:" is label 4, not 8.
#include <metal_stdlib>
using namespace metal;
constant int n = 3;
kernel void kern(device int* out [[buffer(0)]], uint tid [[thread_position_in_grid]]) { int r = 0; { constexpr int n = 7; constexpr int m = ::n + 1; switch (int(tid) + 4) { case m: r = 1; break; default: r = 2; } } out[0] = r; }
