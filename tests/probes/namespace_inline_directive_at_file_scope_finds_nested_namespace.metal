// EXPECT: valid
// DISASM: OpStore
// "inline namespace I { using namespace X; }" at file scope: Q::v is X::Q::v.
#include <metal_stdlib>
using namespace metal;
namespace X { namespace Q { constant float v = 7; } } inline namespace I { using namespace X; } kernel void kern(device float* out [[buffer(0)]]) { out[0] = Q::v; }
