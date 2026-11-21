// EXPECT: error passing by reference a member or element of a conditional expression is not supported
// Apple: accepts; mslc keeps the conditional refusal
struct S{int a;int b;}; struct Q{float2x2 m;}; void sw(thread float2& x){x=float2(1);} Q mkq(){Q q; return q;}
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; Q q1; S s{1,2}; sw((c ? q1 : Q{}).m[0]); out[0]=1; }
