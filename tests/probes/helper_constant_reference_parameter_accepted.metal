// EXPECT: valid
// DISASM: OpFunctionParameter %float
// DISASM-NOT: OpFunctionParameter %_ptr
// A "constant T &" or "const constant T &" parameter names constant memory, which nothing can change,
// so it is passed as the value (Apple accepts both spellings; the helper took no address space before).
// A scalar file-scope constant, an element of a constant array, a bool, a vector and a reference
// parameter of the caller all bind.
#include <metal_stdlib>
using namespace metal;
constant float K = 2.5f;
constant float KA[3] = {1.0f, 2.0f, 4.0f};
constant bool KB = true;
constant float2 KV = {3.0f, 5.0f};
float plus(constant float &c) { return c + 1.0f; }
float chain(const constant float &c) { return plus(c) * 2.0f; }
float vecsum(const constant float2 &v) { return v.x + v.y; }
bool flip(constant const bool &b) { return !b; }
kernel void k(device float *o [[buffer(0)]]) {
	o[0] = plus(K) + chain(KA[2]) + vecsum(KV) + (flip(KB) ? 1.0f : 0.0f);
}
