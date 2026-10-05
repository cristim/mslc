// EXPECT: error #error directive in this source: don't build this
// The message is printed, and an apostrophe in it is not a lexer error.
#error don't build this
kernel void pp_error_directive_reports_its_message(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
