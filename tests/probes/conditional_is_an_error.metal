// EXPECT: error conditional compilation is not supported
// The line bound must not make a conditional silently disappear: it changes the
// meaning of the code, so it stays a hard error.
#if 1
kernel void conditional_is_an_error(device uint* out [[buffer(0)]],
                                    uint i [[thread_position_in_grid]])
{ out[i] = i; }
#endif
