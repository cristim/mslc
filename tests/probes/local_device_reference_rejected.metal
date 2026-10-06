// EXPECT: error a reference in function scope is not lowered yet
//
// Valid MSL, so it is not reported as an address-space error.
struct Foo { float a; };
kernel void local_device_reference_rejected(device Foo* q [[buffer(0)]])
{ device Foo& r = q[0]; r.a = 1.0; }
