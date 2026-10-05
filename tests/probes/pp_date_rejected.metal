// EXPECT: error "__DATE__" is a builtin of Apple's compiler that mslc does not provide
// A build date makes output depend on when it ran.
kernel void pp_date_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = __DATE__; }
