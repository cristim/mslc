// EXPECT: error is made after the class template
// Refs #385: A::h(v) calls the global namespace A; the alias Q::A of N is another name, so Apple gives 1.0. mslc matches aliases by short name and refuses: a documented over-refusal.
#include <metal_stdlib>
using namespace metal;
namespace A { float h(float x) { return 1.0f; } }
namespace N { float h(float x) { return 5.0f; } }
namespace M { float h(int x) { return 7.0f; } }
namespace Q { namespace A = N; }
template <typename T> struct In { T v; float g() const { return A::h(v); } };
namespace N { using M::h; }
kernel void k(device float* o [[buffer(0)]], constant int& s [[buffer(1)]]) {
	In<int> a; a.v = s; o[0] = a.g();
}
