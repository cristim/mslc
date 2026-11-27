// EXPECT: valid
// DISASM: "pp_import_builtin"
// Same as #include <metal_stdlib>, and a second import or include of it is harmless.
#import <metal_stdlib>
#include <metal_stdlib>
#import <metal_stdlib>
using namespace metal;
kernel void pp_import_builtin(device float* out [[buffer(0)]]) { out[0] = sqrt(4.0f); }
