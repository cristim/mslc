// EXPECT: error expression is not assignable
// Apple: expression is not assignable: one prvalue arm makes the conditional a prvalue.
struct S{int a;int b;};
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; S s{1,2}; (c ? s : S{1,2}).a = 4; }
