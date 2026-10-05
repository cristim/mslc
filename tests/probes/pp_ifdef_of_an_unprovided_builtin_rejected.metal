// EXPECT: error "__has_feature" is a builtin of Apple's compiler that mslc does not provide
// Whether it is defined cannot be answered, and 0 would be invented.
#ifdef __has_feature
#endif
kernel void pp_ifdef_of_an_unprovided_builtin_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
