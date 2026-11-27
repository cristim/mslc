// EXPECT: error __has_include can only be used in an #if or #elif
// Clang rejects it in text too.
#define X __has_include("a")
kernel void pp_has_include_outside_an_if_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = X; }
