// EXPECT: error expression is not assignable
// Apple: expression is not assignable
struct S{int a;int b;};
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; (c ? S{1,2} : S{3,4}).a = 5; }
