// EXPECT: error reference to "v" is ambiguous
// Apple: a directive in X pulls Z::v to the global scope, where ::v is also found, and A's
// directive for X makes that visible transitively.
#include <metal_stdlib>
using namespace metal;
constant float v = 1.0;
namespace Z { constant float v = 4.0; }
namespace X { using namespace Z; }
namespace A { using namespace X; float g() { return v; } }
kernel void kern(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = A::g(); }
