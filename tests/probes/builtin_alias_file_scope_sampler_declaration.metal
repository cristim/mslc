// EXPECT: valid
// A file-scope "M::sampler gs;" through an alias of metal starts a declaration (Apple accepts).
#include <metal_stdlib>
namespace M = ::metal; M::sampler gs; kernel void kern(device float* out [[buffer(0)]]) { out[0] = 1; }
