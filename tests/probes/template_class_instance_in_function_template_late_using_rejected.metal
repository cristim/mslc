// EXPECT: error a using declaration or directive is made after the class template
// Refs #341: slice 5d: a using directive below the class template, in a scope it reads names from, may rebind a name W spells (Apple reads ::K from the definition); the instance is refused.
#include <metal_stdlib>
using namespace metal;
constant int K = 5;
template <typename T> struct W { T v; T get() const { return v + T(K); } };
namespace A { constant int K = 7; }
using namespace A;
template <typename T> float h(T x) { W<T> w; w.v = x; return float(w.get()); }
kernel void k(device float* out [[buffer(0)]]) { out[0] = h(1); }
