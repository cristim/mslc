// EXPECT: error unterminated argument list invoking macro "ONE"
// A call that never closes does not swallow the rest of the file.
#define ONE(a) a
kernel void pp_macro_unterminated_call_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = ONE(1; }
