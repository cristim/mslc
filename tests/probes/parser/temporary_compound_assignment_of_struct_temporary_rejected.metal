// EXPECT: error no viable overloaded compound assignment
// Apple: no viable overloaded '+='
struct S{int a;int b;};
kernel void k(device int* out [[buffer(0)]]) { S{1,2} += S{1,2}; }
