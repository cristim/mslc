// EXPECT: error is a bool reference parameter
// A bool is stored as a byte; reading or storing it through the bare reference
// would use the wrong width, so it is refused until the byte mapping exists.
kernel void bool_reference_parameter_rejected(device float *o [[buffer(0)]],
                                              constant bool &b [[buffer(1)]]) {
  o[0] = b ? 1.0 : 0.0;
}
