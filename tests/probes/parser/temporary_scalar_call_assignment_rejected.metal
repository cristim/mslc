// EXPECT: error expression is not assignable
// Apple: expression is not assignable
int one(){ return 1; }
kernel void k(device int* out [[buffer(0)]]) { one() = 3; }
