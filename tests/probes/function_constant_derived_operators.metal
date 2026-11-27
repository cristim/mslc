// EXPECT: valid
// DISASM-MATCH: OpSpecConstantOp %bool IEqual
// DISASM-MATCH: OpSpecConstantOp %bool LogicalNot
// DISASM-MATCH: OpSpecConstantOp %bool LogicalOr
// DISASM-MATCH: OpSpecConstantOp %int Select
// DISASM-MATCH: OpSpecConstantOp %uint ISub
// DISASM-MATCH: OpSpecConstantOp %bool UGreaterThan
//
// Derived function constants built with comparisons, logical operators, the conditional operator and unsigned
// arithmetic, one reading another. On an RTX 3060, with kMode, kAligned and kCount given 2, true, 5, the kernel
// writes 101023; with 3, true, 1 it writes 31; with 3, false, 4 it writes 101031.5.
#include <metal_stdlib>
using namespace metal;
constant int kTwo = 2;
constant int kMode [[function_constant(0)]];
constant bool kAligned [[function_constant(1)]];
constant uint kCount [[function_constant(2)]];
constant bool kModeTwo = kMode == kTwo;
constant bool kUnaligned = !kAligned;
constant bool kUseA = kModeTwo || kUnaligned;
constant int kScaled = kMode * 10 + (kAligned ? 1 : 0);
constant uint kLess = kCount - 1u;
constant bool kMany = kCount > 3;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  float r = float(kScaled);
  if (kUseA) r += 1000.0f;
  if (kMany) r += 100000.0f;
  out[i] = r + float(kLess) * 0.5f;
}
