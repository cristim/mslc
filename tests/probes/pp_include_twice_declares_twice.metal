// EXPECT: error pp_twice_marker
// #import does not change a plain #include: with no #import the header is read
// twice, so the struct it declares is declared twice and the second declaration
// collides with the first.
//
// This is the behaviour pp_include_twice_still_reads_twice was named for. That
// probe showed it through a duplicate-kernel error, which it can no longer do
// now that a module may carry several kernels; here the collision is a struct
// instead of a kernel, which several kernels does not change.
//
// Both declarations come from the two includes. Nothing is declared outside
// them, so if the second include were skipped the module would carry one
// declaration and this probe would fail rather than pass.
#include "include/pp_twice_marker.h"
#include "include/pp_twice_marker.h"