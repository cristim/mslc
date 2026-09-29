// EXPECT: valid
// The other end of the same case: a '#' as the last thing in the file, with no
// name and no trailing newline, is still the null directive rather than a
// directive with a missing name.
kernel void null_directive_at_eof(device uint* out [[buffer(0)]],
                                  uint i [[thread_position_in_grid]])
{ out[i] = i; }
#
