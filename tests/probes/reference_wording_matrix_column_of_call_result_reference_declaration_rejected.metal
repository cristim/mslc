// EXPECT: error the reference "r" is bound to a place mslc cannot name
// Apple: accepts: a matrix operator[] gives an lvalue even on a prvalue; mslc keeps a refusal that does not blame Apple
struct S{int a;int b;}; struct Q{float2x2 m;}; void sw(thread float2& x){x=float2(1);} Q mkq(){Q q; return q;}
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; Q q1; S s{1,2}; thread float2& r = mkq().m[0]; r.x = 1.0f; out[0]=1; }
