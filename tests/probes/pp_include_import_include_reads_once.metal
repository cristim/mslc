// EXPECT: valid
// DISASM: "pp_include_import_include"
// Apple: the #import after an #include reads nothing, and still marks the file, so
// the #include after it reads nothing either. A second read would redefine the struct.
#include "include/pp_import_struct.h"
#import "include/pp_import_struct.h"
#include "include/pp_import_struct.h"

kernel void pp_include_import_include(device int* out [[buffer(0)]])
{
    PpImportStruct s;
    s.a = 3;
    out[0] = s.a;
}
