// EXPECT: error the preprocessed source is larger than
// A doubling chain: 21 macros, one use, 2 million tokens, past the output limit.
#define E0 x
#define E1 E0 E0
#define E2 E1 E1
#define E3 E2 E2
#define E4 E3 E3
#define E5 E4 E4
#define E6 E5 E5
#define E7 E6 E6
#define E8 E7 E7
#define E9 E8 E8
#define E10 E9 E9
#define E11 E10 E10
#define E12 E11 E11
#define E13 E12 E12
#define E14 E13 E13
#define E15 E14 E14
#define E16 E15 E15
#define E17 E16 E16
#define E18 E17 E17
#define E19 E18 E18
#define E20 E19 E19
#define E21 E20 E20
E21
