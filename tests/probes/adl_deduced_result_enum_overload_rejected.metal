// EXPECT: error argument-dependent lookup for "get" would search its namespace
// As adl_deduced_result_overload_found_by_adl_rejected with an enum argument: M::pick returns M::E, so C++
// calls M::get (100).
#include <metal_stdlib>
using namespace metal;
namespace M { enum E { a = 3 }; struct S { float v; }; inline E pick(int x, E e) { return e; } inline float get(E e) { return 100.0f; } }
template <typename T> static inline T pick(T x, M::E e) { return x; }
inline float get(int x) { return 1.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = get(pick(1, M::a)); }
