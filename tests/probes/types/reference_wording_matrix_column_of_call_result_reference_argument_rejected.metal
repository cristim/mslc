// EXPECT: error a member or an index has to start at a name mslc knows
// Apple: accepts; mslc refuses without claiming a temporary
struct S{int a;int b;}; struct Q{float2x2 m;}; void sw(thread float2& x){x=float2(1);} Q mkq(){Q q; return q;}
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; Q q1; S s{1,2}; sw(mkq().m[1]); out[0]=1; }
