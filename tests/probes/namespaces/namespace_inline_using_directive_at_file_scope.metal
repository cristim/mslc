// EXPECT: valid
// DISASM: OpStore
// "inline namespace I { using namespace X; }" at file scope: v is X::v.
#include <metal_stdlib>
using namespace metal;
namespace X { constant float v = 7; } inline namespace I { using namespace X; } kernel void kern(device float* out [[buffer(0)]]) { out[0] = v; }
