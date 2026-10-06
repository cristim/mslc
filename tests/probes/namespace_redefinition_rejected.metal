// EXPECT: error redefinition of "N::k"
#include <metal_stdlib>
using namespace metal;
namespace N { constant float k = 1.0; }
namespace N { constant float k = 2.0; }
