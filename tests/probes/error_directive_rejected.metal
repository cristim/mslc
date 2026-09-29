// EXPECT: error #error directive in this source
// #error exists to fail the build. mslc cannot evaluate the conditional that
// would have guarded one, so a reached #error is an error rather than something
// to discard. xcrun metal rejects it too.
#error "this path is not supported"
kernel void error_directive_rejected(device uint* out [[buffer(0)]],
                                     uint i [[thread_position_in_grid]])
{ out[i] = i; }
