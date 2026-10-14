// EXPECT: error pp_twice_marker
// #import does not change a plain #include: with no #import the header is read
// twice, so a declaration inside it is declared twice and the second
// declaration collides with the first.
//
// This is the behaviour pp_include_twice_still_reads_twice was named for. That
// probe showed it through a duplicate-kernel error, which it can no longer do
// now that a module may carry several kernels; here the collision is a struct
// instead, which is unchanged by that.
struct pp_twice_marker { int x; };
#include "include/pp_twice_marker.h"
#include "include/pp_twice_marker.h"