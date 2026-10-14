// EXPECT: error two entry points are named
// Two entry points cannot share a name in one module: Vulkan identifies them
// by name within an execution model, and spirv-val rejects the module with
// "Entry points cannot share the same name and ExecutionMode".
//
// This is reachable since a module may carry several kernels. A header that
// holds a kernel and is included twice declares that kernel twice, which is
// exactly the shape this header has.
#include "include/pp_import_kernel.h"
#include "include/pp_import_kernel.h"