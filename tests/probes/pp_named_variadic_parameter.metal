// EXPECT: valid
// DISASM: UMin
// GNU "args..." names the variable arguments args instead of __VA_ARGS__.
#define CALL(fn, args...) fn(args)
kernel void pp_named_variadic_parameter(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = CALL(min, i, 10u); }
