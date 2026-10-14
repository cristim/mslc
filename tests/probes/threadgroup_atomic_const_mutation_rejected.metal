// EXPECT: error the const variable "a" needs an initialiser
kernel void k(device uint *o [[buffer(0)]]) {
  threadgroup const atomic_uint a;
  atomic_store_explicit(&a, 1u, memory_order_relaxed);
}
