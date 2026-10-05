// EXPECT: valid
// DISASM: OpConstant %uint 9292
// The operands of ## are not macro-expanded first.
#define A 1
#define JOIN(a, b) a##b
kernel void pp_paste_operand_is_not_expanded(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ uint AB = 9292u; out[i] = JOIN(A, B); }
