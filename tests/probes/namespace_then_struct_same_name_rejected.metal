// EXPECT: error redefinition of "A": declared as namespace and again as struct
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; int f() { return 11; } }
struct A { float v; };
