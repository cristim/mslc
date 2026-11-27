// EXPECT: valid
// static_assert at file scope, in a function body and in a function template (checked per instance), with
// and without a message ([dcl.pre]).
#include <metal_stdlib>
using namespace metal;
static_assert(sizeof(int) == 4, "int");
inline float f(float x) { static_assert(sizeof(float) == 4, "float"); return x; }
template <typename T> inline T g(T x) { static_assert(sizeof(T) <= 4, "T must be small"); static_assert(sizeof(T) >= 1); return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { static_assert(true, "k"); out[i] = f(1.0f) + g(2.0f); }
