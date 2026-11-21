// EXPECT: error does not convert to it
// An enumerator of an anonymous enum is not of the named enum E (Apple refuses the binding).
#include <metal_stdlib>
using namespace metal;
enum E { X, Y };
typedef enum { AX, AY } Anon;
constant Anon KA = AY;
int f(const constant E &x) { return 1; }
kernel void k(device int *o [[buffer(0)]]) { o[0] = f(KA); }
