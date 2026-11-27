// EXPECT: error a using declaration or directive is made after the class template
// A directive for a namespace not nominated before, written after a class template in a scope it reads
// names from, is still refused (template_class_repeated_using_directive is the repeated case).
#include <metal_stdlib>
using namespace metal;
namespace other { constant float scale = 2.0f; }
namespace ns {
template <typename T> struct Box { T v; T w; };
}
using namespace ns;
template <typename T> Box<T> make(T x) { Box<T> b; b.v = x; b.w = x * T(3); return b; }
using namespace other;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { Box<float> b = make<float>(float(i)); out[i] = b.w + scale; }
