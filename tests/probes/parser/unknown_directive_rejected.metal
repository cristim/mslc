// EXPECT: error unknown preprocessor directive "#not_a_directive"
// A preprocessor rejects a directive it does not know, and so does Apple's.
// mslc used to discard it, so a shader whose intent was lost compiled anyway.
#not_a_directive
kernel void unknown_directive_rejected(device uint* out [[buffer(0)]],
                                       uint i [[thread_position_in_grid]])
{ out[i] = i; }
