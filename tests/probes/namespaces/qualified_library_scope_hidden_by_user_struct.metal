// EXPECT: valid
// DISASM: OpFunctionCall
// N brings metal in, but its own struct precise is what N::precise names: 42, not the library's.
#include <metal_stdlib>
using namespace metal;
namespace N { using namespace metal; struct precise { static float sqrt(float a){return 42;} }; } kernel void kern(device float* out [[buffer(0)]]) { out[0] = N::precise::sqrt(4.0f); }
