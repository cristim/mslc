// RUN: grid=64 local=8 seed=1 cases=3
#include <metal_stdlib>
using namespace metal;

struct Pair { float a; uint b; };

// Helpers read through device, const device and constant pointer parameters at indices that are not
// the thread's own, so a helper that read a different element, or the wrong buffer, changes the output.
static float pick(constant float* table, uint i) { return table[(i * 7u + 3u) % 16u]; }
static float pickDevice(const device float* v, uint i) { return v[(i * 5u + 1u) % 64u]; }
static float inner(const device Pair* p, uint i) { return p[i].a + float(p[i].b); }
static float outer(const device Pair* p, uint i) { return inner(p, (i + 9u) % 64u); }

kernel void helper_pointer_parameters(device float* out [[buffer(0)]],
                                      constant float* table [[buffer(1)]],
                                      const device float* v [[buffer(2)]],
                                      const device Pair* pairs [[buffer(3)]],
                                      uint g [[thread_position_in_grid]])
{
	out[g] = pick(table, g) + pickDevice(v, g) * 2.0 + outer(pairs, g);
}
