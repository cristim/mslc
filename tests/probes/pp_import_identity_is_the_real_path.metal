// EXPECT: valid
// DISASM: "pp_import_kernel"
// The same file under three spellings is one file.
#import "include/pp_import_kernel.h"
#import "./include/pp_import_kernel.h"
#import "include/../include/pp_import_kernel.h"
#include "include/../include/./pp_import_kernel.h"
