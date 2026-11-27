// EXPECT: valid
// A '#' with nothing after it on its own line is the null directive, which C
// defines and xcrun metal accepts. Reading a directive name without the line
// bound took the next line's first token for the name, so this file was
// rejected with 'unknown preprocessor directive "#kernel"' and blamed a kernel
// it never mentioned.
#
kernel void null_directive_ignored(device uint* out [[buffer(0)]],
                                    uint i [[thread_position_in_grid]])
{ out[i] = i; }
