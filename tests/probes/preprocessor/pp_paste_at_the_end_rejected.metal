// EXPECT: error '##' cannot appear at either end of a macro expansion
// ## needs two operands.
#define BAD(a) a ##
kernel void pp_paste_at_the_end_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
