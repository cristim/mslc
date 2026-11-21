// EXPECT: error incompatible depth attribute combination
struct O { float d [[point_size, depth(less)]]; float4 c [[color(0)]]; };
fragment O f() { O o; return o; }
