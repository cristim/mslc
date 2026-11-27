// EXPECT: error "__builtin_inff" takes no arguments
// Apple: "too many arguments to function call, expected 0, have 1".
kernel void builtin_infinity_with_argument_rejected(device float *o [[buffer(0)]]) {
  o[0] = __builtin_inff(1);
}
