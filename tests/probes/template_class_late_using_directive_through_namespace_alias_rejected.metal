// EXPECT: error is made after the class template
// Refs #385: as the using declaration probe, with a using directive in N below the template. Apple and master both give 1.0 here, so this refuses a valid program; it is kept because the direct N::h form is refused the same way.
#include <metal_stdlib>
using namespace metal;
namespace N { float h(float x) { return 1.0f; } }
namespace M { float h(int x) { return 7.0f; } }
namespace A = N;
template <typename T> struct In { T v; float g() const { return A::h(v); } };
namespace N { using namespace M; }
kernel void k(device float* o [[buffer(0)]], constant int& s [[buffer(1)]]) { In<int> a; a.v = s; o[0] = a.g(); }
