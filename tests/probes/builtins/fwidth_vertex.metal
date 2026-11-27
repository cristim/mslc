// EXPECT: error fwidth is only available in fragment functions
struct Out { float4 position [[position]]; };
vertex Out main() { Out o; o.position = float4(fwidth(1.0f)); return o; }
