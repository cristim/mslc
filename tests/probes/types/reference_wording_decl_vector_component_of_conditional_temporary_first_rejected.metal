// EXPECT: error is bound to a component of a vector, which a non-const reference cannot name
// Apple: refuses (as a temporary)
struct S{int a;int b;float4 v;}; void one(thread int& x){x=1;} void inc(thread int& x){x++;} void mf(thread float& x){x=1;}
void two(thread int& x, thread int& y){x=1;y=2;} S mk(){S s{1,2,float4(0)};return s;}
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; S s{1,2,float4(0)}, t{3,4,float4(1)}; S vv, ww; thread float& r = (c ? S{1,2} : s).v.x; r = 1; out[0]=1; }
