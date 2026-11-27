// EXPECT: error unexpected character '@'
// A stray character that reaches the parser is still diagnosed.
kernel void pp_stray_character_in_code_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = @; }
