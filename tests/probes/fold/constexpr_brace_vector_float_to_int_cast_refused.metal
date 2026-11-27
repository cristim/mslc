// EXPECT: error a constexpr brace initializer requires a supported constant expression
// Apple accepts this; mslc refuses it on purpose (see known-issues.md, #160), so the probe pins the refusal.
kernel void k(device float* out [[buffer(0)]]) { constexpr int3 v{int3(1.5f)}; out[0] = v.x; }
