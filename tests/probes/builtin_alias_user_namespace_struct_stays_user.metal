// EXPECT: valid
// DISASM: OpStore
// M names the user's U, so M::sampler is U's struct, not the library's sampler.
#include <metal_stdlib>
namespace U { struct sampler { float x; }; }
namespace M = U;
kernel void kern(device float* out [[buffer(0)]]) { M::sampler s; s.x = 3.0; out[0] = s.x; }
