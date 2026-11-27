// EXPECT: error the reference "r" is bound to a place mslc cannot name
// Apple: accepts: a matrix operator[] gives an lvalue even on a prvalue; mslc keeps a refusal that does not blame Apple
float2x2 mkm(){float2x2 m; return m;}
void sw(thread float2& x){x=float2(1);}
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; float2x2 m1; thread float2& r = mkm()[0]; r.x = 1.0f; out[0]=1; }
