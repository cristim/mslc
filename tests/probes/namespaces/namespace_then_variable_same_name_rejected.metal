// EXPECT: error redefinition of "A": declared as namespace and again as variable
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; int f() { return 11; } }
constant int A = 2;
