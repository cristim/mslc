// EXPECT: error a member or element of a conditional expression is not supported
// Apple: accepts; mslc keeps a specific refusal
struct S{int a;int b;}; void m(thread int& x){x=1;} int f(int x,int y){return x;}
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; int n = nn; S s{1,2}, t{3,4}; S p[2]; (c ? p[0] : p[1]).a = 5; }
