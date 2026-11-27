// EXPECT: valid
// thread on a struct local, with a brace list, a direct brace list and no initialiser.
struct Pair { float x; int y; };
kernel void thread_local_struct(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  thread Pair a = {1, 2};
  thread Pair b{3, 4};
  thread Pair c;
  c.x = 5;
  o[i] = a.x + b.y + c.x;
}
