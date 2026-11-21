// EXPECT: valid
// DISASM: OpReturnValue %float_7
#include <metal_stdlib>
namespace N {
  namespace metal { float clamp(float a, float b, float c) { return 7.0f; } }
  float g() { using namespace metal; return clamp(5.0f, 0.0f, 1.0f); }
}
kernel void kern(device int *o [[buffer(0)]]) { o[0] = int(N::g()); }
