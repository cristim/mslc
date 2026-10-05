// EXPECT: error unexpected # "#" in an expression
// Only a '#' that starts its line begins a directive.
kernel void pp_hash_in_the_middle_of_a_line_is_not_a_directive(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1; # define NOT_A_DIRECTIVE 1 }
