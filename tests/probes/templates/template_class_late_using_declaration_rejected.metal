// EXPECT: error a using declaration or directive is made after the class template
// Refs #341: Apple binds M::K from the definition; a using declaration below the template must not rebind K to X::K.
#include <metal_stdlib>
using namespace metal;
namespace X { constant int K = 2; }
namespace M { constant int K = 1; namespace I { template <typename T> struct P { T v; float f() const { return float(K) + float(v); } }; } }
namespace M { namespace I { using X::K; } }
kernel void k(device float* out [[buffer(0)]]) { M::I::P<float> p; p.v = 0.5f; out[0] = p.f(); }
