// EXPECT: valid
// DISASM: "pp_import_kernel"
// Apple: an imported file is not read again by a later #include.
#import "include/pp_import_kernel.h"
#include "include/pp_import_kernel.h"
