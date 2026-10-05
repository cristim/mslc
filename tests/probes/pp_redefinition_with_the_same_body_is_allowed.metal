// EXPECT: valid
// DISASM: OpIAdd
// C allows a macro to be redefined to an identical body.
#define SAME  (1   +  2)
#define SAME (1 + 2)
kernel void pp_redefinition_with_the_same_body_is_allowed(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = SAME; }
