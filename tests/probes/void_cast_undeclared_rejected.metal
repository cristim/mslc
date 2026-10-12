// EXPECT: error "nosuch" is not a parameter, local, constant or builtin
// Apple: "use of undeclared identifier 'nosuch'".
kernel void void_cast_undeclared_rejected(device float *o [[buffer(0)]]) {
  (void)nosuch;
}
