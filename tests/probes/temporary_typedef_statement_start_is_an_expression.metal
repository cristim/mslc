// EXPECT: error expression is not assignable
// Apple: expression is not assignable (and "T{1,2};" alone is accepted)
struct S{int a;int b;}; typedef S T;
kernel void k(device int* out [[buffer(0)]]) { T{1,2}; T{1,2}.a = 3; }
