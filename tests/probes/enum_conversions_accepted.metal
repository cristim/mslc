// EXPECT: valid
// What Apple accepts: its own enumerator, a cast, a copy, a conditional of its enumerators,
// the file-scope constant of the type, a helper result, and reading it as an integer.
enum Mode { A, B, C };
constant Mode kDefault = B;
struct S { Mode m; int n; };
Mode pick(int i) { return i > 0 ? A : B; }
kernel void enum_conversions_accepted(device int *o [[buffer(0)]], uint u [[thread_position_in_grid]]) {
  bool g = u > 0;
  Mode m = A;
  m = B;
  m = (Mode)1;
  m = static_cast<Mode>(u);
  Mode n = m;
  Mode k = g ? A : B;
  m = g ? C : m;
  Mode d = kDefault;
  m = pick(1);
  S s;
  s.m = A;
  s.n = A;
  s.m = k;
  int i = m;
  uint j = n;
  float f = m;
  bool b = m == 1;
  int sum = m + 1;
  o[0] = i + int(j) + sum + int(f) + int(b) + int(d) + s.n;
}
