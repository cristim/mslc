// EXPECT: valid
// DISASM: %float_9
// DISASM-NOT: %float_3
// Refs #341: a dependent call is also looked up through the argument's namespace where the template is
// instantiated, which clang puts at the end of the file: A::h(SA), declared below the template and
// below the call, beats the global h(float) for a struct argument. Apple calls A::h.
#include <metal_stdlib>
using namespace metal;
namespace A { struct SA { float a; }; }
float h(float x) { return 3.0f; }
template <typename T> float f(T x) { return h(x); }
kernel void k(device float* out [[buffer(0)]]) { A::SA s; s.a = 1.0f; out[0] = f(s); }
namespace A { float h(SA x) { return 9.0f; } }
