// EXPECT: error [[sample_mask]] on an input parameter is not lowered yet
fragment float4 f(uint m [[sample_mask]]) { return float4(0.75); }
