// EXPECT: error gives the template parameter "T" a pointer, array, resource, enum or volatile type, which is not lowered
// A call cannot give an enum as a template argument either, and an instance for an enum would not be told
// apart from the one for its underlying type: f<int> must stay the primary template's.
#include <metal_stdlib>
using namespace metal;
enum E { A = 1 };
template <typename T> float f(T x) { return 1.0f; }
template <> float f(E x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f<int>(A); }
