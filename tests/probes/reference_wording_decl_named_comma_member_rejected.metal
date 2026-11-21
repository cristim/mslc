// EXPECT: error binding a reference to a member or element of a comma expression is not lowered yet
// Apple: accepts; mslc keeps a specific refusal
struct S{int a;int b;float4 v;}; void one(thread int& x){x=1;} void inc(thread int& x){x++;} void mf(thread float& x){x=1;}
void two(thread int& x, thread int& y){x=1;y=2;} S mk(){S s{1,2,float4(0)};return s;}
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; S s{1,2,float4(0)}, t{3,4,float4(1)}; S vv, ww; thread int& r = (0, s).a; r = 1; out[0]=1; }
