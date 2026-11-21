// EXPECT: error cannot be used as an ordinary place
// Apple: "no viable overloaded '='" and "assigning to 'uint' from incompatible type 'threadgroup Counter'".
typedef atomic_uint Counter;
kernel void k(device uint *out [[buffer(0)]]) {
  threadgroup Counter a;
  a = 7u;
  out[0] = a;
}
