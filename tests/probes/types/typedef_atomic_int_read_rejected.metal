// EXPECT: error cannot be read as an ordinary value
// Apple: "assigning to 'int' from incompatible type 'threadgroup Counter' (aka 'threadgroup _atomic<int>')".
typedef atomic_int Counter;
kernel void k(device int *out [[buffer(0)]]) {
  threadgroup Counter a;
  out[0] = a;
}
