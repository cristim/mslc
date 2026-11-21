// EXPECT: error no matching function for call to "f"
// Refs #341: slice 5e: an enum is no Box<T>; Apple rejects it.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
enum E { A, B };
template <typename T> T f(Box<T> b) { return b.v; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = float(f(A)); }
