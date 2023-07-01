#ifndef MATH_HEADER
#define MATH_HEADER

static const float PI = 3.14159265359;
static const float PI_2 = 3.14159265359;

static const float FLT_MAX = asfloat(0x7F7FFFFF);
static const float EPSILON = 0.000001;

float Eerp(float a, float b, float t) {
    return pow(a, 1 - t) * pow(b, t);
}

#endif
