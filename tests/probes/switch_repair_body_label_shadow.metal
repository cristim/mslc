// EXPECT: error case value is not a constant expression
constant int label = 1;
kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  switch (gid) {
    case 0: break;
    int label;
    case label: out[0] = 2; break;
  }
}
