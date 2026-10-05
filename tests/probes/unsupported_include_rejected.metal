// EXPECT: error #include "local_defs.h" not found
//
// A quoted include used to be refused outright, because mslc had no preprocessor
// and dropping it left the program compiling up to the first name the header
// would have declared. It is searched for now, so what this pins is a header
// that is not there: the include is an error, never a skipped line.
//
// Not checked with xcrun metal: it fails the same way because
// tests/probes/local_defs.h does not exist.
#include "local_defs.h"
//
// mslc used to discard every #include, so a quoted one was discarded the same
// way: no diagnostic, and the source compiled on up to the first name the header
// would have declared. An app writing "#include <metal_stdlib>" got a library
// back and then failed on its first metal:: identifier, which is the worst shape
// of failure because the library handle was already handed over. Measured
// through VibeDarling/darling-metal#7.
//
// The name is in the diagnostic because the caller has to know which header to
// stop writing. The one include mslc honours is <metal_stdlib>, whose contents
// it already has built in, and only in the angled form: a quoted include
// searches the includer's own directory in MSL, so an app shipping a file of that
// name would have it ignored here, which is the same failure this probe pins.
// <cstdint> is not honoured either, and is the case worth getting right: it is
// not an MSL header at all (xcrun metal reports "file not found"), and mslc is
// missing four of the typedefs it declares, intptr_t, uintptr_t, intmax_t and
// uintmax_t, so honouring it would accept source Apple's compiler rejects.
//
// The quoted form is the case here because it has no '>' to stop at, so the end
// of the line is the only bound. The old probe for that, which asserted the
// include was ignored, pinned the behaviour this removes.
//
// Not checked with xcrun metal: Apple's compiler opens the include and fails
// because tests/probes/local_defs.h does not exist, which is the same answer
// mslc now gives for a different reason.
#include "local_defs.h"
kernel void unsupported_include_rejected(device uint* out [[buffer(0)]],
                                        uint i [[thread_position_in_grid]])
{ out[i] = i; }
