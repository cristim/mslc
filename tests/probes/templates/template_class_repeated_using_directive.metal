// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// A using directive for a namespace its scope already nominates, written after a class template, changes no
// lookup ([namespace.udir]), so the instance a function template names is made as without it.
#include <metal_stdlib>
using namespace metal;
namespace ns {
template <typename T> struct Box { T v; T w; };
}
using namespace ns;
template <typename T> Box<T> make(T x) { Box<T> b; b.v = x; b.w = x * T(3); return b; }
using namespace metal;
using namespace ns;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { Box<float> b = make<float>(float(i)); out[i] = b.w + 1.0f; }
