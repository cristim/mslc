// EXPECT: error a using declaration or directive is made after the class template
// Refs #341: M::I reads K through a directive for M::I::Y, and a using declaration added to Y later rebinds it (Apple: M::K).
#include <metal_stdlib>
using namespace metal;
namespace X { constant int K = 2; }
namespace M { constant int K = 1; namespace I { namespace Y { } using namespace Y; template <typename T> struct P { T v; float f() const { return float(K) + float(v); } }; } }
namespace M { namespace I { namespace Y { using X::K; } } }
kernel void k(device float* out [[buffer(0)]]) { M::I::P<float> p; p.v = 0.5f; out[0] = p.f(); }
