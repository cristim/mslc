// EXPECT: valid
// DISASM: UMin
// __VA_ARGS__ takes the rest of the argument list, commas included.
#define CALL(fn, ...) fn(__VA_ARGS__)
kernel void pp_variadic_macro(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = CALL(min, i, 10u); }
