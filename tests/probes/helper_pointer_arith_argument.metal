// EXPECT: valid
// Pointer arithmetic as a pointer argument: p + n, &b[i], n + p and the
// negative p - n all name the element the helper starts at, and the helper's
// own p[i] steps by the element's Metal size, in bytes.
#include <metal_stdlib>
using namespace metal;
float readMid(device float* p, uint i) { return p[i]; }
float readFirst(device float* p) { return *p; }
kernel void helper_pointer_arith_argument(device float* out [[buffer(0)]], device float* in [[buffer(1)]],
                                          uint i [[thread_position_in_grid]])
{
    out[i] = readMid(in + i, 0) + readMid(&in[i], 0) + readMid(in - 1, i + 1) + readFirst(in);
}
