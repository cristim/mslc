// EXPECT: valid
// DISASM: NClamp
// "using namespace M;" with M an alias of ::metal is a directive for the library.
#include <metal_stdlib>
namespace M = ::metal;
using namespace M;
kernel void kern(device float* out [[buffer(0)]]) { out[0] = clamp(5.0f,0.0f,1.0f); }
