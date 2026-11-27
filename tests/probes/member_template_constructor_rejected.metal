// EXPECT: error constructor template "S::S" is declared and never defined
// Refs #341: a constructor template without a body is refused where it is declared.
#include <metal_stdlib>
using namespace metal;
struct S { float a; template <typename U> S(U x); };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 0.0f; }
