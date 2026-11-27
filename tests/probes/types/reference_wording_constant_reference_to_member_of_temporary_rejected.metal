// EXPECT: error the reference "r" is bound to a place mslc cannot name
// Apple: binding reference of type 'const constant int' to value of type 'int' changes address space; mslc words it as an unnamable place
struct S{int a;int b;}; struct Q{float2x2 m;}; void sw(thread float2& x){x=float2(1);} Q mkq(){Q q; return q;}
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; Q q1; S s{1,2}; constant int& r = S{1,2}.a; out[0]=1; }
