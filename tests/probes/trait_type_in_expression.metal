// EXPECT: valid
// RUN: grid=1
// A transformation trait names a type wherever one can start an expression or a statement: a functional cast
// make_unsigned_t<Y>(y), a C cast (make_unsigned_t<int>)x, and a declaration make_scalar_t<T> s. ANGLE's shift
// helper is the first function: a shift by 32 or more, or by a negative count, gives 0. On an RTX 3060 out is
// 1048576 0 0 4294967294 7 4.
#include <metal_stdlib>
using namespace metal;
template <typename X, typename Y>
X rshift(X x, Y y) { return select(X(0), x >> (y & Y(31)), make_unsigned_t<Y>(y) < make_unsigned_t<Y>(32)); }
template <typename T> uint first(T v) { make_scalar_t<T> s = v.x; return s; }
kernel void trait_type_in_expression(device uint* out [[buffer(0)]]) {
    out[0] = rshift(0x10000000u, 8u);
    out[1] = uint(rshift(5, 40));
    out[2] = uint(rshift(5, -1));
    out[3] = (make_unsigned_t<int>)-2;
    make_unsigned_t<int> u = 7;
    out[4] = u;
    out[5] = first(uint2(4, 9));
}
