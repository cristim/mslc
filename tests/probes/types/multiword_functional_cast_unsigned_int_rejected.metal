// EXPECT: error functional cast
//
// Apple: expected '(' for function-style cast or type construction. A functional cast takes a single-word type.
kernel void k(device uint* o [[buffer(0)]]) { float x = 1; o[0] = unsigned int(x); }
