// EXPECT: valid
// The directives mslc discards rather than rejects. xcrun metal accepts
// #pragma, #undef and #line; the guard is that rejecting an unknown one did not
// start rejecting these.
// DISASM: OpCapability Shader
#pragma pack(1)
#undef ACCEPTED_DIRECTIVES_IGNORED
#line 1 "ignored.metal"
kernel void accepted_directives_ignored(device uint* out [[buffer(0)]],
                                        uint i [[thread_position_in_grid]])
{ out[i] = i; }
