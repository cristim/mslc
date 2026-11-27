// EXPECT: error fwidth is only available in fragment functions
kernel void main() { fwidth(1.0f); }
