// EXPECT: valid
// DISASM: "pp_import_kernel"
// Apple: #import of a file that was already #included reads nothing.
#include "include/pp_import_kernel.h"
#import "include/pp_import_kernel.h"
