// EXPECT: valid
// DISASM: OpStore
// A using directive inside an inline namespace is its enclosing namespace's too: A::z finds
// X::z (Apple stores 7).
#include <metal_stdlib>
using namespace metal;
namespace X { constant float z = 7; } namespace A { inline namespace I { using namespace X; } } kernel void kern(device float* out [[buffer(0)]]) { out[0] = A::z; }
