// EXPECT: error write ".template f<...>(...)"
// Refs #386: a local whose type is the template parameter is dependent; Apple rejects the bare form ("use 'template'
// keyword to treat 'f' as a dependent template name").
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
template <typename T> float g(T in) { T copy = in; return copy.f<float>(1.0f); }
kernel void k(device float* o [[buffer(0)]]) { B y{2.0f}; o[0] = g(y); }
