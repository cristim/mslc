// EXPECT: valid
// A pragma mslc does not act on is accepted and dropped, as xcrun metal accepts
// it. #undef of a name never defined and #line are honoured with no visible
// effect. The guard is that rejecting an unknown directive did not start
// rejecting these.
// DISASM: OpCapability Shader
#pragma pack(1)
#undef ACCEPTED_DIRECTIVES_IGNORED
#line 1 "ignored.metal"
kernel void accepted_directives_ignored(device uint* out [[buffer(0)]],
                                        uint i [[thread_position_in_grid]])
{ out[i] = i; }
