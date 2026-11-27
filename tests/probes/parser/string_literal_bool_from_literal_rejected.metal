// EXPECT: error bool-from-pointer are not lowered
// Apple accepts bool b = "x"; mslc rejects it by design (known-issues.md, #156).
#include <metal_stdlib>
using namespace metal;
kernel void string_literal_bool_from_literal_rejected(device float *o [[buffer(0)]]) {
  bool b = "x";
  o[0] = b;
}
