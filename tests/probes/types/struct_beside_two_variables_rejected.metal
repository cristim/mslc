// EXPECT: error redefinition of "x": declared as variable and again as variable
#include <metal_stdlib>
using namespace metal;
struct x {};
constant int x = 2;
constant int x = 3;
