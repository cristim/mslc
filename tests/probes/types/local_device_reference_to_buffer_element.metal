// EXPECT: valid
//
// Valid MSL, so it is not reported as an address-space error: a device
// reference to a buffer element names that element.
struct Foo { float a; };
kernel void local_device_reference_to_buffer_element(device Foo* q [[buffer(0)]])
{ device Foo& r = q[0]; r.a = 1.0; }
