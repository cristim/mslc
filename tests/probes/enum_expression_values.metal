// EXPECT: valid
// DISASM: OpConstant %int 4
// DISASM: OpConstant %int 5
// DISASM: OpConstant %int 6
// DISASM: OpConstant %int 2
// DISASM: OpSNegate %int
//
// An enumerator may be any integer constant expression of literals and earlier enumerators.
#include <metal_stdlib>
using namespace metal;
enum { A = 1 << 2, B = A + 1, C = (B * 2) - A, D = ~0, E = -3, F = 10 % 4 };
kernel void enum_expression_values(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[0] = A; out[1] = B; out[2] = C; out[3] = D; out[4] = E; out[5] = F; }
