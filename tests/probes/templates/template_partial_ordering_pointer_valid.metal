// EXPECT: valid
// DISASM: %float_2 = OpConstant %float 2
// DISASM: %float_4 = OpConstant %float 4
// DISASM: %float_22 = OpConstant %float 22
// DISASM-NOT: %float_1 = OpConstant %float 1
// DISASM-NOT: %float_3 = OpConstant %float 3
// DISASM-NOT: %float_11 = OpConstant %float 11
// Refs #341: Apple picks h(device T*) over h(T), c(device const T*) over c(device T*) on a const buffer,
// and m(device T*, T) over m(device T*, U).
#include <metal_stdlib>
using namespace metal;
template <typename T> float h(T x) { return 1.0f; }
template <typename T> float h(device T* p) { return 2.0f; }
template <typename T> float c(device T* p) { return 3.0f; }
template <typename T> float c(device const T* p) { return 4.0f; }
template <typename T, typename U> float m(device T* p, U x) { return 11.0f; }
template <typename T> float m(device T* p, T x) { return 22.0f; }
kernel void k(device float* out [[buffer(0)]], device const float* cin [[buffer(1)]],
              uint i [[thread_position_in_grid]])
{
    out[i] = h(out) + c(cin) + m(out, 5.0f);
}
