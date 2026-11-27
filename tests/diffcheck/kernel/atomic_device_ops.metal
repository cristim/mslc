// RUN: grid=64 local=16 cases=3
// Device atomic_uint / atomic_int (mutable and const) across load, store, exchange, compare_exchange_weak and
// all fetch_* ops (per-thread slots for non-commutative ops, commutative reductions across threads), plus
// threadgroup atomic_uint / atomic_int locals. as_type avoids signed overflow UB on edge inputs.
#include <metal_stdlib>
using namespace metal;

kernel void atomic_device_ops(
    device atomic_uint *au [[buffer(0)]],
    device atomic_int *ai [[buffer(1)]],
    device const atomic_uint *cau [[buffer(2)]],
    device uint *out [[buffer(3)]],
    device int *iout [[buffer(4)]],
    uint g [[thread_position_in_grid]],
    uint l [[thread_position_in_threadgroup]],
    uint tg [[threadgroup_position_in_grid]]) {
    uint uslot = 16u + g;
    atomic_store_explicit(&au[uslot], atomic_load_explicit(&cau[g], memory_order_relaxed) ^ g, memory_order_relaxed);
    atomic_fetch_sub_explicit(&au[uslot], g + 1u, memory_order_relaxed);
    uint old_ex = atomic_exchange_explicit(&au[uslot], atomic_load_explicit(&au[uslot], memory_order_relaxed) + 7u, memory_order_relaxed);
    uint exp_ok = atomic_load_explicit(&au[uslot], memory_order_relaxed);
    bool cas_hit = atomic_compare_exchange_weak_explicit(&au[uslot], &exp_ok, exp_ok ^ 0x55u, memory_order_relaxed, memory_order_relaxed);
    uint exp_miss = 0u;
    atomic_compare_exchange_weak_explicit(&au[uslot], &exp_miss, 999u, memory_order_relaxed, memory_order_relaxed);
    out[g] = old_ex + (cas_hit ? 1u : 0u) + exp_miss;

    // Unsigned vs signed min/max on MSB-set slots so SMin/UMin or SMax/UMax confusion always shows.
    uint uminmax = 80u + g;
    atomic_store_explicit(&au[uminmax], 0x80000000u + g, memory_order_relaxed);
    atomic_fetch_min_explicit(&au[uminmax], 10u, memory_order_relaxed);
    uint umaxslot = 144u + g;
    atomic_store_explicit(&au[umaxslot], 10u, memory_order_relaxed);
    atomic_fetch_max_explicit(&au[umaxslot], 0x80000000u + g, memory_order_relaxed);

    // Commutative reductions across all 64 invocations into au[0..5] and ai[0..3].
    atomic_fetch_add_explicit(&au[0], g + 1u, memory_order_relaxed);
    atomic_fetch_xor_explicit(&au[1], (g * 2654435761u) ^ 0x12345678u, memory_order_relaxed);
    atomic_fetch_or_explicit(&au[2], 1u << (g & 15u), memory_order_relaxed);
    atomic_fetch_and_explicit(&au[3], ~(1u << (g & 7u)), memory_order_relaxed);
    atomic_fetch_min_explicit(&au[4], g * 3u + 5u, memory_order_relaxed);
    atomic_fetch_max_explicit(&au[5], g * 7u + 1u, memory_order_relaxed);

    atomic_fetch_add_explicit(&ai[0], int(g) - 31, memory_order_relaxed);
    atomic_fetch_min_explicit(&ai[1], -int(g) - 10, memory_order_relaxed);
    atomic_fetch_max_explicit(&ai[2], int(g) - 5, memory_order_relaxed);
    atomic_fetch_xor_explicit(&ai[3], int(g * 17u), memory_order_relaxed);

    int islot = 16 + int(g);
    int inext = as_type<int>(as_type<uint>(atomic_load_explicit(&ai[islot], memory_order_relaxed)) + 3u);
    int iold = atomic_exchange_explicit(&ai[islot], inext, memory_order_relaxed);
    atomic_fetch_sub_explicit(&ai[islot], 2, memory_order_relaxed);
    int iexp = atomic_load_explicit(&ai[islot], memory_order_relaxed);
    bool icas = atomic_compare_exchange_weak_explicit(&ai[islot], &iexp, as_type<int>(as_type<uint>(iexp) - 9u), memory_order_relaxed, memory_order_relaxed);
    iout[g] = as_type<int>(as_type<uint>(iold) + (icas ? 1u : 0u));

    int iminmax = 80 + int(g);
    atomic_store_explicit(&ai[iminmax], -5 - int(g), memory_order_relaxed);
    atomic_fetch_min_explicit(&ai[iminmax], 10, memory_order_relaxed);
    int imaxslot = 144 + int(g);
    atomic_store_explicit(&ai[imaxslot], -5 - int(g), memory_order_relaxed);
    atomic_fetch_max_explicit(&ai[imaxslot], -200, memory_order_relaxed);

    threadgroup atomic_uint tgu;
    threadgroup atomic_int tgi;
    if (l == 0u) {
        atomic_store_explicit(&tgu, 100u, memory_order_relaxed);
        atomic_store_explicit(&tgi, -20, memory_order_relaxed);
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    atomic_fetch_add_explicit(&tgu, l + 1u, memory_order_relaxed);
    atomic_fetch_min_explicit(&tgi, -int(l) * 3, memory_order_relaxed);
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (l == 0u) {
        out[64u + tg] = atomic_load_explicit(&tgu, memory_order_relaxed);
        iout[64 + int(tg)] = atomic_load_explicit(&tgi, memory_order_relaxed);
    }
}
