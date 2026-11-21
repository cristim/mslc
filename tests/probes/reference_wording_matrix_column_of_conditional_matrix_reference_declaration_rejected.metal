// EXPECT: error binding a reference to a member or element of a conditional expression is not supported
// Apple: accepts; mslc keeps the conditional refusal
float2x2 mkm(){float2x2 m; return m;}
void sw(thread float2& x){x=float2(1);}
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; float2x2 m1; thread float2& r = (c ? m1 : mkm())[0]; r.x = 1.0f; out[0]=1; }
