// EXPECT: error no viable overloaded
// Apple: no viable overloaded '='
struct S{int a;int b;}; struct T{int a;int b;};
kernel void k(device int* out [[buffer(0)]]) { S{1,2} = T{3,4}; }
