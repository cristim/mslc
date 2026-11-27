// EXPECT: error #error directive in this source
// #error exists to fail the build, so a reached one is an error rather than
// something to discard. xcrun metal rejects it too. One in a skipped group is
// not reached; pp_error_in_a_skipped_group_is_ignored pins that.
#error "this path is not supported"
kernel void error_directive_rejected(device uint* out [[buffer(0)]],
                                     uint i [[thread_position_in_grid]])
{ out[i] = i; }
