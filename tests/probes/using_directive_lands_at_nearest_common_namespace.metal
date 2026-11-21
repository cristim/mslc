// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_2[_0-9]* Aligned
#include <metal_stdlib>
namespace Q { namespace K { constant int y = 1; } }
namespace M { namespace K { constant int y = 2; }
  namespace N { using namespace ::Q; constant int z = K::y; } }
kernel void kern(device int *o [[buffer(0)]]) { o[0] = M::N::z; }
