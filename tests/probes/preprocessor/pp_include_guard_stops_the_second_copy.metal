// EXPECT: valid
// DISASM: OpEntryPoint GLCompute
// DISASM: "pp_guarded_kernel"
// The header defines a kernel, so including it twice without its guard would be a redefinition.
#include "include/pp_guarded_kernel.h"
#include "include/pp_guarded_kernel.h"
