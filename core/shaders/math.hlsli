#ifndef MATH_HEADER
#define MATH_HEADER

// todo: $Globals ? wtf
// const float PI = 3.14159265359;
#define PI  3.14159265359
#define FLT_MAX asfloat(0x7F7FFFFF)

static const float EPSILON = 0.000001;

float Eerp(float a, float b, float t) {
    return pow(a, 1 - t) * pow(b, t);
}

#endif
