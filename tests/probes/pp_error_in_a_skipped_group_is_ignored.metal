// EXPECT: valid
// An #error that is not reached does not fail the build.
#if 0
#error not reached
#endif
kernel void pp_error_in_a_skipped_group_is_ignored(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
