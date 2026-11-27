// EXPECT: error no matching function for call to "mf": argument 1 of the call to "mf" is a temporary
// Apple: no matching function for call to 'mf'
struct S{int a;int b;float4 v;}; void one(thread int& x){x=1;} void inc(thread int& x){x++;} void mf(thread float& x){x=1;}
void two(thread int& x, thread int& y){x=1;y=2;} S mk(){S s{1,2,float4(0)};return s;}
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; S s{1,2,float4(0)}, t{3,4,float4(1)}; S vv, ww; mf((c ? S{1,2} : S{3,4}).v.x); out[0]=1; }
