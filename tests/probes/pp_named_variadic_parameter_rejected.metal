// EXPECT: error named variadic macro parameters
// GNU "args..." is not supported; "..." is.
#define BAD(args...) args
kernel void pp_named_variadic_parameter_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
