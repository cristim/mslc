// EXPECT: error a kernel returns void
struct O { uint m [[sample_mask]]; };
kernel O f() { O o; return o; }
