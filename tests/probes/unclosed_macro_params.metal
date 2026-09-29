// EXPECT: error expected ) to close a macro parameter list
// A '#define FOO(' with no closing paren must not scan on into the next line.
// The parameter list is bounded by the directive's line like every other part of
// it, so the unclosed paren is reported where it is. Before the bound it stopped
// at the ')' inside "[[buffer(0)]]" further down and reported that instead.
#define FOO(
kernel void unclosed_macro_params(device uint* out [[buffer(0)]],
                                  uint i [[thread_position_in_grid]])
{ out[i] = i; }
