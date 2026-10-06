// EXPECT: valid
// DISASM: "pp_import_kernel"
// A header that is imported twice is read once; without that this is a redefinition.
#import "include/pp_import_kernel.h"
#import "include/pp_import_kernel.h"
