// EXPECT: error atomic_store_explicit returns void and has no value
// Apple: "cannot initialize a variable of type uint with an rvalue of type void".
kernel void atomic_store_as_value_rejected(device uint *o [[buffer(0)]], device atomic_uint *c [[buffer(1)]]) {
  o[0] = atomic_store_explicit(c, 5u, memory_order_relaxed);
}
