// EXPECT: error no struct declares a member function template
//
// A name after 'template' that no struct declares as a member template is refused with that
// diagnostic, not with the receiver's refusal, inside a function template body (#386).
struct B {
    float h(float x) const { return x; }
};

template <typename U>
float g(U x)
{
    B y;
    return y.template nope(x);
}

kernel void member_template_undeclared_keyword_rejected(device float *o [[buffer(0)]])
{
    o[0] = g(1.0f);
}
