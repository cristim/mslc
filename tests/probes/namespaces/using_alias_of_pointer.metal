// EXPECT: valid
// RUN: grid=1 local=1
//
// "using P = device float*;" names the pointer type, address space included: a helper parameter,
// an entry point argument and sizeof. Apple accepts every line.
#include <metal_stdlib>
using namespace metal;
struct S { float a; float b; };
using P = device float*;
using CP = constant float*;
using KP = const device float*;
using SP = device S*;
float rd(P p, uint i) { return p[i]; }
float rk(KP p) { return p[0] + p[1]; }
kernel void using_alias_of_pointer(P o [[buffer(0)]], CP c [[buffer(1)]], SP s [[buffer(2)]]) {
  o[1] = c[0] + 1.0;
  o[2] = rd(o, 1) + rk(o) + s[0].b;
  o[3] = sizeof(P) + sizeof(CP);
}
