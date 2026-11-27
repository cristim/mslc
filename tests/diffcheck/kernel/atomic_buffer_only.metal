// RUN: grid=64 local=16 cases=3
// A kernel whose bindings are strictly atomic buffers (typedef aliases plus const atomic read): Apple reflects
// _atomic<T> with field __s, layout cross-checks both, and only the atomic buffers are compared and perturbed.
#include <metal_stdlib>
using namespace metal;

typedef atomic_uint AtomicU;
typedef atomic_int AtomicI;

kernel void atomic_buffer_only(
    device AtomicU *au [[buffer(0)]],
    device AtomicI *ai [[buffer(1)]],
    device const atomic_uint *cau [[buffer(2)]],
    uint g [[thread_position_in_grid]]) {
    uint prev = atomic_load_explicit(&cau[g], memory_order_relaxed);
    atomic_fetch_add_explicit(&au[g], (prev & 0xffu) + 1u, memory_order_relaxed);
    atomic_fetch_xor_explicit(&ai[g], int(prev) ^ int(g), memory_order_relaxed);
}
