// EXPECT: error a template argument that is a pointer, an array, a resource, an enum, volatile
// Refs #341: f<volatile float> is a different instance in Apple (is_same<T, float> is false); the
// qualifier is not modelled, so the argument is refused and the template is never instantiated as float.
#include <metal_stdlib>
using namespace metal;
template <typename T, typename = enable_if_t<is_same_v<T, float>>> int f(T) { return 1; }
template <typename T, typename = enable_if_t<!is_same_v<T, float>>, typename = void> int f(T) { return 2; }
kernel void k(device int* out [[buffer(0)]]) { out[0] = f<volatile float>(1.0f); }
