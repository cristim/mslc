// EXPECT: error expected a type, found [
struct O { uint m[1] [[sample_mask]]; };
fragment O f() { O o; return o; }
