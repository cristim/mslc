// EXPECT: error attribute "thread_position_in_grid" cannot appear more than once on a declaration
kernel void parameter_builtin_attribute_repeated_rejected(uint a [[thread_position_in_grid, thread_position_in_grid]], device float *o [[buffer(0)]]) {
  o[a] = 1.0;
}
