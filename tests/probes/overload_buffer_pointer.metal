// EXPECT: valid
// DISASM: OpFunctionCall
// A whole buffer chooses among overloads taking device or constant pointers: it reaches exactly the pointer to its
// element type in its address space, a const one by a qualification conversion, which loses to the pointer
// without it ([over.ics.rank]); a const buffer reaches only the const pointer. On an RTX 3060 thread i gets
// 256 + 1000, 117837012 + 2000 (as a float), 2 + 3000, 1, 5 and 6 with the buffers filled with bytes 0, 1, 2, ...
#include <metal_stdlib>
using namespace metal;
inline float get(const device ushort* p, uint i) { return float(p[i]) + 1000.0f; }
inline float get(const device uint* p, uint i) { return float(p[i]) + 2000.0f; }
inline float get(const device uchar* p, uint i) { return float(p[i]) + 3000.0f; }
inline float w(device float* p) { return 1.0f; }
inline float w(const device float* p) { return 2.0f; }
inline float c(constant float* p) { return 5.0f; }
inline float c(const device float* p) { return 6.0f; }
kernel void k(device float* out [[buffer(0)]], const device ushort* a [[buffer(1)]], const device uint* b [[buffer(2)]],
              const device uchar* u [[buffer(3)]], constant float* cf [[buffer(4)]], uint i [[thread_position_in_grid]]) {
  const float r[6] = { get(a, i), get(b, i), get(u, i), w(out), c(cf), c(out) };
  out[i] = r[i % 6];
}
