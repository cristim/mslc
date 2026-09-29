// EXPECT: valid
// mslc discards every #include, so a quoted one has to be discarded the same
// way. It has no '>' to stop at, so the end of the line is the only bound, and
// without one it consumed the rest of the file.
//
// Not checked with xcrun metal: Apple's compiler opens the include and fails
// because tests/probes/local_defs.h does not exist. mslc is specified to
// treat includes as no-ops (a real preprocessor is M1 item 8).
#include "local_defs.h"
kernel void quoted_include_ignored(device uint* out [[buffer(0)]],
                                   uint i [[thread_position_in_grid]])
{ out[i] = i; }
