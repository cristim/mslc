// EXPECT: error expression is not assignable
// Apple: expression is not assignable
struct S{int a;int b;}; struct O{S s;int z;}; void m(thread int& x){x=1;} S mk(){S s{1,2};return s;}
kernel void k(device int* out [[buffer(0)]]) { O{S{1,2},3}.s.b = 3; }
