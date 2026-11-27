// EXPECT: valid
// DISASM: OpConstant %uint 7171
// DISASM: OpConstant %uint 7272
// DISASM: OpConstant %uint 7373
// Without the recursion guard, value -> value -> ... never ends.
// A and B name each other: A expands to B, which expands to A, which stays. Each ends up as itself.
#define value value
#define A B
#define B A
kernel void pp_macro_does_not_expand_inside_itself(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ uint value = 7171u; uint A = 7272u; uint B = 7373u; out[i] = value + A + B; }
