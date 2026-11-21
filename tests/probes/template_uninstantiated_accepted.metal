// EXPECT: valid
// DISASM-NOT: OpFunctionCall
// Refs #341: a template nobody instantiates is parsed and accepted, as clang does,
// whatever its body says about its dependent types. None of these reaches SPIR-V.
#include <metal_stdlib>
using namespace metal;

template <typename V>
float luminance(thread const V &c)
{
    return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
}

template <typename T, int N = 4, typename _E = typename enable_if<is_floating_point<T>::value>::type>
inline __attribute__((__always_inline__)) T scaled(T x) { T acc = x; for (int i = 0; i < N; ++i) acc += x; return acc; }

template <typename A, typename B = vec<A, 2>> B only_declared(A a);

template <typename T, typename = typename enable_if<is_floating_point<typename make_scalar<T>::type>::value>::type>
T nested_close(T x) { return x; }

namespace util {
template <typename T> T twice(T x) { return x + x; }
}

kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1.0f; }

template <int N, typename T>
typename enable_if<(N > 2), T>::type paren_in_return_type(T x) { return x; }
