// EXPECT: valid
// DISASM: OpConstant %int 4242
// An object-like macro is replaced by its body.
#define ANSWER 4242
kernel void pp_object_macro(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = ANSWER; }
