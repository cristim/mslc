// EXPECT: error "h" is called before it is declared
// Refs #341: a name that does not depend on U is bound where the struct is; h below it is not found (Apple: undeclared).
#include <metal_stdlib>
using namespace metal;
struct S { template <typename U> float f(U x) const { return h(1); } };
float h(int x) { return 1.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ S s; out[i] = s.f(2.0f); }
