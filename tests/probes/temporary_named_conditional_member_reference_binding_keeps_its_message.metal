// EXPECT: error binding a reference to a member or element of a conditional expression
// Apple: accepts; mslc keeps a specific refusal
struct S{int a;int b;}; void m(thread int& x){x=1;} int f(int x,int y){return x;}
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; int n = nn; S s{1,2}, t{3,4}; S p[2]; thread int& r = (c ? s : t).a; r = 1; }
