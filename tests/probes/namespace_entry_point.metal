// EXPECT: valid
// DISASM: "N::k"
//
// Apple names a kernel declared in a namespace by its qualified name.
#include <metal_stdlib>
using namespace metal;
namespace N { kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0; } }
