// EXPECT: error a source may declare one kernel
// #import does not change a plain #include: with no #import the second copy is read.
#include "include/pp_import_kernel.h"
#include "include/pp_import_kernel.h"
