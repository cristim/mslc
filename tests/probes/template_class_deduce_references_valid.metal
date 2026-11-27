// EXPECT: valid
// RUN: grid=4 local=4
// Refs #341: slice 5e: a const reference and a mutable reference to Box<T> from a thread local, and a by-value parameter from a device lvalue.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> T unbox(const thread Box<T>& b) { return b.v; }
template <typename T> void put(thread Box<T>& b, T x) { b.v = x; }
template <typename T> T copy(Box<T> b) { return b.v; }
kernel void k(device float* out [[buffer(0)]], device Box<float>* io [[buffer(1)]], uint i [[thread_position_in_grid]])
{
	Box<float> a; a.v = 1.5f + float(i);
	put(a, a.v + 1.0f);
	io[i].v = 3.0f + float(i);
	out[3 * i] = unbox(a);
	out[3 * i + 1] = copy(io[i]);
	out[3 * i + 2] = a.v;
}
