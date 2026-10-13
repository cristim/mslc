// EXPECT: valid
// In namespace metal, parameter lists the library does not have (clamp of an int2, length of
// a scalar, sin of an int, mix of float, int, float, max of a struct) and a prototype of one it
// has: Apple accepts all of them unused. Only a definition with one of the library's parameter
// lists is a redefinition. (One per name: a second signature of one name is #266.)
#include <metal_stdlib>
struct P { int v; };
namespace metal {
    int clamp(int2 a) { return 7; }
    float length(float x) { return x; }
    int sin(int x) { return x; }
    float mix(float x, int y, float a) { return x; }
    P max(P x, P y) { return x; }
    float fma(float a, float b, float c);
}
kernel void namespace_metal_overloads_the_library_lacks(device int* out [[buffer(0)]])
{ out[0] = 1; }
