// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// Single-element array<Struct, 1> initializers with typed S{...} elements (local,
// double-braced, single-field struct, designated, constexpr, file-scope constant, nested).
#include <metal_stdlib>
using namespace metal;
struct Pair { float x; float y; };
struct One { float x; };
constant array<Pair, 1> kFileConst = { Pair{ .x = 2.0f, .y = 3.0f } };
constexpr constant array<Pair, 1> kFileConstexpr = { array<Pair, 1>{ Pair{ .x = 4.0f, .y = 5.0f } } };
constant array<Pair, 1> kFileDirect = array<Pair, 1>{ Pair{ .x = 1.0f, .y = 2.0f } };
kernel void array_std_struct_single_element_init_valid(device float* out [[buffer(0)]],
                                                       device const float* in [[buffer(1)]],
                                                       uint i [[thread_position_in_grid]]) {
	array<Pair, 1> a = { Pair{ in[i], 2.0f } };
	array<Pair, 1> a2 = {{ Pair{ 3.0f, in[i + 1] } }};
	array<One, 1> b = { One{ in[i] + 1.0f } };
	array<Pair, 1> d = { Pair{ .x = 7.0f, .y = in[i + 2] } };
	constexpr array<Pair, 1> lc = { Pair{ 1.5f, 2.5f } };
	array<array<Pair, 1>, 1> n1 = { array<Pair, 1>{ Pair{ 6.0f, 7.0f } } };
	array<array<Pair, 1>, 1> n3 = {{{ Pair{ 8.0f, 9.0f } }}};
	array<array<Pair, 1>, 2> n2 = {{ { array<Pair, 1>{ Pair{ 1.0f, 2.0f } } }, { array<Pair, 1>{{ Pair{ 3.0f, 4.0f } }} } }};
	array<Pair, 1> copy = { array<Pair, 1>{{ Pair{ 1.25f, 2.75f } }} };
	out[i * 4 + 0] = a[0].x + a[0].y + a2[0].x + a2[0].y + copy[0].x + copy[0].y + n2[0][0].x + n2[0][0].y + n2[1][0].x + n2[1][0].y;
	out[i * 4 + 1] = b[0].x + d[0].x + d[0].y + lc[0].x + lc[0].y;
	out[i * 4 + 2] = kFileConst[0].x + kFileConst[0].y + kFileConstexpr[0].x + kFileConstexpr[0].y + kFileDirect[0].x + kFileDirect[0].y;
	out[i * 4 + 3] = n1[0][0].x + n1[0][0].y + n3[0][0].x + n3[0][0].y;
}
