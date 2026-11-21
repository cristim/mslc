// EXPECT: error the trait "is_convertible" is not lowered
// Refs #341: only the predicates whose value was measured against Apple are folded; any other
// would have to be guessed at, and a guess decides which overload is called.
#include <metal_stdlib>
using namespace metal;
template <typename T, typename E = typename enable_if<is_convertible<T, float>::value>::type> float f(T x) { return 1.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.0f); }
