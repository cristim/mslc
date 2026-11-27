// EXPECT: error is bound to a member or element of a temporary, which a non-const reference cannot bind
// Apple: non-const lvalue reference to type 'int' cannot bind to a temporary of type 'int'
struct S{int a;int b;float4 v;}; void one(thread int& x){x=1;} void inc(thread int& x){x++;} void mf(thread float& x){x=1;}
void two(thread int& x, thread int& y){x=1;y=2;} S mk(){S s{1,2,float4(0)};return s;}
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; S s{1,2,float4(0)}, t{3,4,float4(1)}; S vv, ww; thread int& r = (c ? S{1,2} : S{3,4}).a; r = 1; out[0]=1; }
