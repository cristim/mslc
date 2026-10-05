// EXPECT: error "__has_feature" is a builtin of Apple's compiler that mslc does not provide
// Named, never evaluated as 0.
#if __has_feature(cxx_rtti)
#endif
kernel void pp_has_feature_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
