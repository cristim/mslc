// EXPECT: error parameter "p" needs a device or constant address space to be a buffer binding, not thread
struct Foo { float a; };
kernel void buffer_thread_reference_without_attribute_rejected(thread Foo& p) { float x = p.a; }
