// EXPECT: error macro name missing in #define
// Macro definitions need a name.
#define
kernel void pp_define_without_a_name_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
