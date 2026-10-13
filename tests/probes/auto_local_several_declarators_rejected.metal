// EXPECT: error several declarators after auto are not lowered yet
// Apple: "auto deduced as int in declaration of z and as float in declaration of w" when they differ.
kernel void auto_local_several_declarators_rejected(device float *o [[buffer(0)]]) {
  auto x = 1, y = 2;
  o[0] = x + y;
}
