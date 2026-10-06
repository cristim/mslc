// EXPECT: valid
// DISASM: "pp_import_kernel"
// Two headers import the same file, and the source imports both.
#import "include/pp_import_a.h"
#import "include/pp_import_b.h"
constant uint pp_import_sum = PP_IMPORT_A + PP_IMPORT_B;
