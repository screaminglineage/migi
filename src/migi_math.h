#ifndef MIGI_MATH_H
#define MIGI_MATH_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "migi_core.h"

// Relative and absolute Tolerances
typedef struct {
    double rel_tol;
    double abs_tol;
} IsCloseOpt;

// From: https://docs.python.org/3/whatsnew/3.5.html#pep-485-a-function-for-testing-approximate-equality
static bool isclose_opt(double a, double b, IsCloseOpt opt) {
    return fabs(a - b) <= migi_max(opt.rel_tol * migi_max(fabs(a), fabs(b)), opt.abs_tol);
}

// Default values
#define isclose(a, b, ...) isclose_opt((a), (b), \
        (IsCloseOpt){ .rel_tol = 1e-09, .abs_tol = 0.0, __VA_ARGS__ })


// Vectors
// 2D
typedef struct {
    union {
        struct {
            int32_t x;
            int32_t y;
        };
        int32_t v[2];
    };
} Vec2I32;

typedef struct {
    union {
        struct {
            int64_t x;
            int64_t y;
        };
        int64_t v[2];
    };
} Vec2I64;

typedef struct {
    union {
        struct {
            float x;
            float y;
        };
        float v[2];
    };
} Vec2F;

typedef struct {
    union {
        struct {
            double x;
            double y;
        };
        double v[2];
    };
} Vec2D;


// 3D
typedef struct {
    union {
        struct {
            int32_t x;
            int32_t y;
            int32_t z;
        };
        struct {
            int32_t r;
            int32_t g;
            int32_t b;
        };
        struct {
            Vec2I32 xy;
            int32_t _z;
        };
        struct {
            int32_t _x;
            Vec2I32 yz;
        };
        int32_t v[3];
    };
} Vec3I32;

typedef struct {
    union {
        struct {
            int64_t x;
            int64_t y;
            int64_t z;
        };
        struct {
            int64_t r;
            int64_t g;
            int64_t b;
        };
        struct {
            Vec2I64 xy;
            int64_t _z;
        };
        struct {
            int64_t _x;
            Vec2I64 yz;
        };
        int64_t v[3];
    };
} Vec3I64;


typedef struct {
    union {
        struct {
            float x;
            float y;
            float z;
        };
        struct {
            float r;
            float g;
            float b;
        };
        struct {
            Vec2F xy;
            float _z;
        };
        struct {
            float _x;
            Vec2F yz;
        };
        float v[3];
    };
} Vec3F;

typedef struct {
    union {
        struct {
            double x;
            double y;
            double z;
        };
        struct {
            double r;
            double g;
            double b;
        };
        struct {
            Vec2D xy;
            double _z;
        };
        struct {
            double _x;
            Vec2D yz;
        };
        double v[3];
    };
} Vec3D;


// 4D
typedef struct {
    union {
        struct {
            int32_t x;
            int32_t y;
            int32_t z;
            int32_t w;
        };
        struct {
            int32_t r;
            int32_t g;
            int32_t b;
            int32_t a;
        };
        struct {
            int32_t up;
            int32_t down;
            int32_t left;
            int32_t right;
        };
        struct {
            Vec2I32 xy;
            Vec2I32 zw;
        };
        struct {
            int32_t __x;
            Vec2I32 yz;
            int32_t __w;
        };
        struct {
            Vec3I32 xyz;
            int32_t _w;
        };
        struct {
            int32_t _x;
            Vec3I32 yzw;
        };
        int32_t v[4];
    };
} Vec4I32;

typedef struct {
    union {
        struct {
            int64_t x;
            int64_t y;
            int64_t z;
            int64_t w;
        };
        struct {
            int64_t r;
            int64_t g;
            int64_t b;
            int64_t a;
        };
        struct {
            int64_t up;
            int64_t down;
            int64_t left;
            int64_t right;
        };
        struct {
            Vec2I64 xy;
            Vec2I64 zw;
        };
        struct {
            int64_t __x;
            Vec2I64 yz;
            int64_t __w;
        };
        struct {
            Vec3I64 xyz;
            int64_t _w;
        };
        struct {
            int64_t _x;
            Vec3I64 yzw;
        };
        int64_t v[4];
    };
} Vec4I64;


typedef struct {
    union {
        struct {
            float x;
            float y;
            float z;
            float w;
        };
        struct {
            float r;
            float g;
            float b;
            float a;
        };
        struct {
            float up;
            float down;
            float left;
            float right;
        };
        struct {
            Vec2F xy;
            Vec2F zw;
        };
        struct {
            float __x;
            Vec2F yz;
            float __w;
        };
        struct {
            Vec3F xyz;
            float _w;
        };
        struct {
            float _x;
            Vec3F yzw;
        };
        float v[4];
    };
} Vec4F;

typedef struct {
    union {
        struct {
            double x;
            double y;
            double z;
            double w;
        };
        struct {
            double r;
            double g;
            double b;
            double a;
        };
        struct {
            double up;
            double down;
            double left;
            double right;
        };
        struct {
            Vec2D xy;
            Vec2D zw;
        };
        struct {
            double __x;
            Vec2D yz;
            double __w;
        };
        struct {
            Vec3D xyz;
            double _w;
        };
        struct {
            double _x;
            Vec3D yzw;
        };
        double v[4];
    };
} Vec4D;


// Matrices
// 2x2
typedef struct {
    union {
        int64_t m[2][2];
        Vec2I64 rows[2];
    };
} Mat2x2I64;

typedef struct {
    union {
        int32_t m[2][2];
        Vec2I32 rows[2];
    };
} Mat2x2I32;

typedef struct {
    union {
        float m[2][2];
        Vec2F rows[2];
    };
} Mat2x2F;

typedef struct {
    union {
        double m[2][2];
        Vec2D rows[2];
    };
} Mat2x2D;


// 3x3
typedef struct {
    union {
        int32_t m[3][3];
        Vec3I32 rows[3];
    };
} Mat3x3I32;

typedef struct {
    union {
        int64_t m[3][3];
        Vec3I64 rows[3];
    };
} Mat3x3I64;

typedef struct {
    union {
        float m[3][3];
        Vec3F rows[3];
    };
} Mat3x3F;

typedef struct {
    union {
        double m[3][3];
        Vec3D rows[3];
    };
} Mat3x3D;


// 4x4
typedef struct {
    union {
        int64_t m[4][4];
        Vec4I64 rows[4];
    };
} Mat4x4I64;

typedef struct {
    union {
        int32_t m[4][4];
        Vec4I32 rows[4];
    };
} Mat4x4I32;

typedef struct {
    union {
        float m[4][4];
        Vec4F rows[4];
    };
} Mat4x4F;

typedef struct {
    union {
        double m[4][4];
        Vec4D rows[4];
    };
} Mat4x4D;


// Generic Sized Vectors
typedef struct {
    int32_t *data;
    size_t length;
} VecI32;

typedef struct {
    int64_t *data;
    size_t length;
} VecI64;

typedef struct {
    float *data;
    size_t length;
} VecF;

typedef struct {
    double *data;
    size_t length;
} VecD;

// Generic Sized Matrices
typedef struct {
    int32_t *data;
    size_t rows, cols;
} MatI32;

typedef struct {
    int64_t *data;
    size_t rows, cols;
} MatI64;

typedef struct {
    float *data;
    size_t rows, cols;
} MatF;

typedef struct {
    double *data;
    size_t rows, cols;
} MatD;


// Constructors
#define v2i64(x_, y_) (Vec2I64){.x = x_, .y = y_}
#define v2i32(x_, y_) (Vec2I32){.x = x_, .y = y_}
#define v2f(x_, y_) (Vec2F){.x = x_, .y = y_}
#define v2d(x_, y_) (Vec2D){.x = x_, .y = y_}

#define v3i64(x_, y_, z_) (Vec3I64){.x = x_, .y = y_, .z = z_}
#define v3i32(x_, y_, z_) (Vec3I32){.x = x_, .y = y_, .z = z_}
#define v3f(x_, y_, z_) (Vec3F){.x = x_, .y = y_, .z = z_}
#define v3d(x_, y_, z_) (Vec3D){.x = x_, .y = y_, .z = z_}

#define v4i64(x_, y_, z_, w_) (Vec4I64){.x = x_, .y = y_, .z = z_, .w = w_}
#define v4i32(x_, y_, z_, w_) (Vec4I32){.x = x_, .y = y_, .z = z_, .w = w_}
#define v4f(x_, y_, z_, w_) (Vec4F){.x = x_, .y = y_, .z = z_, .w = w_}
#define v4d(x_, y_, z_, w_) (Vec4D){.x = x_, .y = y_, .z = z_, .w = w_}

// Matrix Constructors
// Each take the value of the diagnonal
#define m2x2i64(d) (Mat2x2I64){.m[0][0] = d, .m[1][1] = d}
#define m2x2i32(d) (Mat2x2I32){.m[0][0] = d, .m[1][1] = d}
#define m2x2f(d) (Mat2x2F){.m[0][0] = d, .m[1][1] = d}
#define m2x2d(d) (Mat2x2D){.m[0][0] = d, .m[1][1] = d}

#define m3x3i64(d) (Mat3x3I64){.m[0][0] = d, .m[1][1] = d, .m[2][2] = d}
#define m3x3i32(d) (Mat3x3I32){.m[0][0] = d, .m[1][1] = d, .m[2][2] = d}
#define m3x3f(d) (Mat3x3F){.m[0][0] = d, .m[1][1] = d, .m[2][2] = d}
#define m3x3d(d) (Mat3x3D){.m[0][0] = d, .m[1][1] = d, .m[2][2] = d}

#define m4x4i64(d) (Mat4x4I64){.m[0][0] = d, .m[1][1] = d, .m[2][2] = d, .m[3][3] = d}
#define m4x4i32(d) (Mat4x4I32){.m[0][0] = d, .m[1][1] = d, .m[2][2] = d, .m[3][3] = d}
#define m4x4f(d) (Mat4x4F){.m[0][0] = d, .m[1][1] = d, .m[2][2] = d, .m[3][3] = d}
#define m4x4d(d) (Mat4x4D){.m[0][0] = d, .m[1][1] = d, .m[2][2] = d, .m[3][3] = d}


// Equality operations
//
// Equality for integer vectors
#define v2i_eq(v1, v2) \
    ((v1).x == (v2).x  \
  && (v1).y == (v2).y)

#define v3i_eq(v1, v2) \
    ((v1).x == (v2).x  \
  && (v1).y == (v2).y  \
  && (v1).z == (v2).z)

#define v4i_eq(v1, v2) \
    ((v1).x == (v2).x  \
  && (v1).y == (v2).y  \
  && (v1).z == (v2).z  \
  && (v1).w == (v2).w)


// Approximate Equality for float vectors
#define v2_isclose(v1, v2)     \
    (isclose((v1).x, (v2).x) \
  && isclose((v1).y, (v2).y))

#define v3_isclose(v1, v2)     \
    (isclose((v1).x, (v2).x) \
  && isclose((v1).y, (v2).y) \
  && isclose((v1).z, (v2).z))

#define v4_isclose(v1, v2)     \
    (isclose((v1).x, (v2).x) \
  && isclose((v1).y, (v2).y) \
  && isclose((v1).z, (v2).z) \
  && isclose((v1).w, (v2).w))


// Equality for integer matrics
#define m2x2i_isclose(m1, m2)          \
    v2i_eq((m1).rows[0], (m2).rows[0]) \
 && v2i_eq((m1).rows[1], (m2).rows[1])

#define m3x3i_isclose(m1, m2)          \
    v3i_eq((m1).rows[0], (m2).rows[0]) \
 && v3i_eq((m1).rows[1], (m2).rows[1]) \
 && v3i_eq((m1).rows[2], (m2).rows[2])

#define m4x4i_isclose(m1, m2)          \
    v4i_eq((m1).rows[0], (m2).rows[0]) \
 && v4i_eq((m1).rows[1], (m2).rows[1]) \
 && v4i_eq((m1).rows[2], (m2).rows[2]) \
 && v4i_eq((m1).rows[3], (m2).rows[3])


// Approximate Equality for float matrices
#define m2x2_isclose(m1, m2)               \
    v2_isclose((m1).rows[0], (m2).rows[0]) \
 && v2_isclose((m1).rows[1], (m2).rows[1])

#define m3x3_isclose(m1, m2)               \
    v3_isclose((m1).rows[0], (m2).rows[0]) \
 && v3_isclose((m1).rows[1], (m2).rows[1]) \
 && v3_isclose((m1).rows[2], (m2).rows[2])

#define m4x4_isclose(m1, m2)               \
    v4_isclose((m1).rows[0], (m2).rows[0]) \
 && v4_isclose((m1).rows[1], (m2).rows[1]) \
 && v4_isclose((m1).rows[2], (m2).rows[2]) \
 && v4_isclose((m1).rows[3], (m2).rows[3])


// Vector operations

// 2D
static Vec2F v2f_id();
static Vec2F v2f_scale(Vec2F a, float s);
static Vec2F v2f_add(Vec2F a, Vec2F b);
static Vec2F v2f_sub(Vec2F a, Vec2F b);
static Vec2F v2f_mul(Vec2F a, Vec2F b);
static Vec2F v2f_div(Vec2F a, Vec2F b);
static float v2f_dot(Vec2F a, Vec2F b);
static float v2f_len(Vec2F v);
static float v2f_len_squared(Vec2F v);
static Vec2F v2f_normalize(Vec2F v);
static Vec2F v2f_lerp(Vec2F a, Vec2F b, float t);
static Vec2F v2f_transform(Vec2F v, Mat2x2F transform);

static Vec2D v2d_id();
static Vec2D v2d_scale(Vec2D a, double s);
static Vec2D v2d_add(Vec2D a, Vec2D b);
static Vec2D v2d_sub(Vec2D a, Vec2D b);
static Vec2D v2d_mul(Vec2D a, Vec2D b);
static Vec2D v2d_div(Vec2D a, Vec2D b);
static double v2d_dot(Vec2D a, Vec2D b);
static double v2d_len(Vec2D v);
static double v2d_len_squared(Vec2D v);
static Vec2D v2d_normalize(Vec2D v);
static Vec2D v2d_lerp(Vec2D a, Vec2D b, double t);
static Vec2D v2d_transform(Vec2D v, Mat2x2D transform);


// 3D
static Vec3F v3f_id();
static Vec3F v3f_scale(Vec3F a, float s);
static Vec3F v3f_add(Vec3F a, Vec3F b);
static Vec3F v3f_sub(Vec3F a, Vec3F b);
static Vec3F v3f_mul(Vec3F a, Vec3F b);
static Vec3F v3f_div(Vec3F a, Vec3F b);
static float v3f_dot(Vec3F a, Vec3F b);
static float v3f_len(Vec3F v);
static float v3f_len_squared(Vec3F v);
static Vec3F v3f_normalize(Vec3F v);
static Vec3F v3f_lerp(Vec3F a, Vec3F b, float t);
static Vec3F v3f_transform(Vec3F v, Mat3x3F transform);

static Vec3D v3d_id();
static Vec3D v3d_scale(Vec3D a, double s);
static Vec3D v3d_add(Vec3D a, Vec3D b);
static Vec3D v3d_sub(Vec3D a, Vec3D b);
static Vec3D v3d_mul(Vec3D a, Vec3D b);
static Vec3D v3d_div(Vec3D a, Vec3D b);
static double v3d_dot(Vec3D a, Vec3D b);
static double v3d_len(Vec3D v);
static double v3d_len_squared(Vec3D v);
static Vec3D v3d_normalize(Vec3D v);
static Vec3D v3d_lerp(Vec3D a, Vec3D b, double t);
static Vec3D v3d_transform(Vec3D v, Mat3x3D transform);

// 3D Cross Products
static Vec3F v3f_cross(Vec3F a, Vec3F b);
static Vec3D v3d_cross(Vec3D a, Vec3D b);


// 4D
static Vec4F v4f_id();
static Vec4F v4f_scale(Vec4F a, float s);
static Vec4F v4f_add(Vec4F a, Vec4F b);
static Vec4F v4f_sub(Vec4F a, Vec4F b);
static Vec4F v4f_mul(Vec4F a, Vec4F b);
static Vec4F v4f_div(Vec4F a, Vec4F b);
static float v4f_dot(Vec4F a, Vec4F b);
static float v4f_len(Vec4F v);
static float v4f_len_squared(Vec4F v);
static Vec4F v4f_normalize(Vec4F v);
static Vec4F v4f_lerp(Vec4F a, Vec4F b, float t);
static Vec4F v4f_transform(Vec4F v, Mat4x4F transform);

static Vec4D v4d_id();
static Vec4D v4d_scale(Vec4D a, double s);
static Vec4D v4d_add(Vec4D a, Vec4D b);
static Vec4D v4d_sub(Vec4D a, Vec4D b);
static Vec4D v4d_mul(Vec4D a, Vec4D b);
static Vec4D v4d_div(Vec4D a, Vec4D b);
static double v4d_dot(Vec4D a, Vec4D b);
static double v4d_len(Vec4D v);
static double v4d_len_squared(Vec4D v);
static Vec4D v4d_normalize(Vec4D v);
static Vec4D v4d_lerp(Vec4D a, Vec4D b, double t);
static Vec4D v4d_transform(Vec4D v, Mat4x4D transform);


// Matrix Operations
// TODO: implement unimplemented functions (complete mat_mul TODO first)
static Mat2x2F m2x2f_scale(Mat2x2F a, float s);
static Mat2x2F m2x2f_mul(Mat2x2F a, Mat2x2F b);
static Mat2x2F m2x2f_transpose(Mat2x2F m);
static float m2x2f_determinant(Mat2x2F m);

static Mat2x2D m2x2d_scale(Mat2x2D a, float s);
static Mat2x2D m2x2d_mul(Mat2x2D a, Mat2x2D b);
static Mat2x2D m2x2d_transpose(Mat2x2D m);
static double m2x2d_determinant(Mat2x2D m);

static Mat3x3F m3x3f_scale(Mat3x3F a, float s);
static Mat3x3F m3x3f_mul(Mat3x3F a, Mat3x3F b);
static Mat3x3F m3x3f_transpose(Mat3x3F m);
static float m3x3f_determinant(Mat3x3F m);

static Mat3x3D m3x3d_scale(Mat3x3D a, float s);
static Mat3x3D m3x3d_mul(Mat3x3D a, Mat3x3D b);
static Mat3x3D m3x3d_transpose(Mat3x3D m);
static double m3x3d_determinant(Mat3x3D m);

static Mat4x4F m4x4f_scale(Mat4x4F a, float s);
static Mat4x4F m4x4f_mul(Mat4x4F a, Mat4x4F b);
static Mat4x4F m4x4f_transpose(Mat4x4F m);
static float m4x4f_determinant(Mat4x4F m);

static Mat4x4D m4x4d_scale(Mat4x4D a, float s);
static Mat4x4D m4x4d_mul(Mat4x4D a, Mat4x4D b);
static Mat4x4D m4x4d_transpose(Mat4x4D m);
static double m4x4d_determinant(Mat4x4D m);


// TODO: check these!!!!!!!

// 2D
// F32
static Vec2F v2f_id() {
    return v2f(1, 1);
}

static Vec2F v2f_scale(Vec2F a, float s) {
    return v2f(a.x * s, a.y * s);
}

static Vec2F v2f_add(Vec2F a, Vec2F b) {
    return v2f(a.x + b.x, a.y + b.y);
}

static Vec2F v2f_sub(Vec2F a, Vec2F b) {
    return v2f(a.x - b.x, a.y - b.y);
}

static Vec2F v2f_mul(Vec2F a, Vec2F b) {
    return v2f(a.x * b.x, a.y * b.y);
}

static Vec2F v2f_div(Vec2F a, Vec2F b) {
    return v2f(a.x / b.x, a.y / b.y);
}

static float v2f_dot(Vec2F a, Vec2F b) {
    return a.x * b.x + a.y * b.y;
}

static float v2f_len_squared(Vec2F v) {
    return v2f_dot(v, v);
}

static float v2f_len(Vec2F v) {
    return sqrtf(v2f_dot(v, v));
}

static Vec2F v2f_normalize(Vec2F v) {
    return v2f_scale(v, 1.0f/v2f_len(v));
}

static Vec2F v2f_lerp(Vec2F a, Vec2F b, float t) {
    return v2f_add(v2f_scale(a, 1 - t), v2f_scale(b, t));
}

static Vec2F v2f_transform(Vec2F v, Mat2x2F transform) {
    Vec2F result = {0};
    for (size_t i = 0; i < 2; i++) {
        result.v[i] = v.x * transform.m[0][i] + v.y * transform.m[1][i];
    }
    return result;
}

// F64
static Vec2D v2d_id() {
    return v2d(1, 1);
}

static Vec2D v2d_scale(Vec2D a, double s) {
    return v2d(a.x * s, a.y * s);
}

static Vec2D v2d_add(Vec2D a, Vec2D b) {
    return v2d(a.x + b.x, a.y + b.y);
}

static Vec2D v2d_sub(Vec2D a, Vec2D b) {
    return v2d(a.x - b.x, a.y - b.y);
}

static Vec2D v2d_mul(Vec2D a, Vec2D b) {
    return v2d(a.x * b.x, a.y * b.y);
}

static Vec2D v2d_div(Vec2D a, Vec2D b) {
    return v2d(a.x / b.x, a.y / b.y);
}

static double v2d_dot(Vec2D a, Vec2D b) {
    return a.x * b.x + a.y * b.y;
}

static double v2d_len_squared(Vec2D v) {
    return v2d_dot(v, v);
}

static double v2d_len(Vec2D v) {
    return sqrt(v2d_dot(v, v));
}

static Vec2D v2d_normalize(Vec2D v) {
    return v2d_scale(v, 1.0f/v2d_len(v));
}

static Vec2D v2d_lerp(Vec2D a, Vec2D b, double t) {
    return v2d_add(v2d_scale(a, 1 - t), v2d_scale(b, t));
}

static Vec2D v2d_transform(Vec2D v, Mat2x2D transform) {
    Vec2D result = {0};
    for (size_t i = 0; i < 2; i++) {
        result.v[i] = v.x * transform.m[0][i] + v.y * transform.m[1][i];
    }
    return result;
}

// 3D
// F32
static Vec3F v3f_id() {
    return v3f(1, 1, 1);
}

static Vec3F v3f_scale(Vec3F a, float s) {
    return v3f(a.x * s, a.y * s, a.z * s);
}

static Vec3F v3f_add(Vec3F a, Vec3F b) {
    return v3f(a.x + b.x, a.y + b.y, a.z + b.z);
}

static Vec3F v3f_sub(Vec3F a, Vec3F b) {
    return v3f(a.x - b.x, a.y - b.y, a.z - b.z);
}

static Vec3F v3f_mul(Vec3F a, Vec3F b) {
    return v3f(a.x * b.x, a.y * b.y, a.z * b.z);
}

static Vec3F v3f_div(Vec3F a, Vec3F b) {
    return v3f(a.x / b.x, a.y / b.y, a.z / b.z);
}

static float v3f_dot(Vec3F a, Vec3F b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float v3f_len_squared(Vec3F v) {
    return v3f_dot(v, v);
}

static float v3f_len(Vec3F v) {
    return sqrtf(v3f_dot(v, v));
}

static Vec3F v3f_normalize(Vec3F v) {
    return v3f_scale(v, 1.0f/v3f_len(v));
}

static Vec3F v3f_lerp(Vec3F a, Vec3F b, float t) {
    return v3f_add(v3f_scale(a, 1 - t), v3f_scale(b, t));
}

static Vec3F v3f_transform(Vec3F v, Mat3x3F transform) {
    Vec3F result = {0};
    for (size_t i = 0; i < 3; i++) {
        result.v[i] = v.x * transform.m[0][i]
             + v.y * transform.m[1][i]
             + v.z * transform.m[2][i];
    }
    return result;
}

static Vec3F v3f_cross(Vec3F a, Vec3F b) {
    return v3f(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}


// F64
static Vec3D v3d_id() {
    return v3d(1, 1, 1);
}

static Vec3D v3d_scale(Vec3D a, double s) {
    return v3d(a.x * s, a.y * s, a.z * s);
}

static Vec3D v3d_add(Vec3D a, Vec3D b) {
    return v3d(a.x + b.x, a.y + b.y, a.z + b.z);
}

static Vec3D v3d_sub(Vec3D a, Vec3D b) {
    return v3d(a.x - b.x, a.y - b.y, a.z - b.z);
}

static Vec3D v3d_mul(Vec3D a, Vec3D b) {
    return v3d(a.x * b.x, a.y * b.y, a.z * b.z);
}

static Vec3D v3d_div(Vec3D a, Vec3D b) {
    return v3d(a.x / b.x, a.y / b.y, a.z / b.z);
}

static double v3d_dot(Vec3D a, Vec3D b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static double v3d_len_squared(Vec3D v) {
    return v3d_dot(v, v);
}

static double v3d_len(Vec3D v) {
    return sqrt(v3d_dot(v, v));
}

static Vec3D v3d_normalize(Vec3D v) {
    return v3d_scale(v, 1.0f/v3d_len(v));
}

static Vec3D v3d_lerp(Vec3D a, Vec3D b, double t) {
    return v3d_add(v3d_scale(a, 1 - t), v3d_scale(b, t));
}

static Vec3D v3d_cross(Vec3D a, Vec3D b) {
    return v3d(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}

static Vec3D v3d_transform(Vec3D v, Mat3x3D transform) {
    Vec3D result = {0};
    for (size_t i = 0; i < 3; i++) {
        result.v[i] = v.x * transform.m[0][i]
             + v.y * transform.m[1][i]
             + v.z * transform.m[2][i];
    }
    return result;
}


// 4D
// F32
static Vec4F v4f_id() {
    return v4f(1, 1, 1, 1);
}

static Vec4F v4f_scale(Vec4F a, float s) {
    return v4f(a.x * s, a.y * s, a.z * s, a.w * s);
}

static Vec4F v4f_add(Vec4F a, Vec4F b) {
    return v4f(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

static Vec4F v4f_sub(Vec4F a, Vec4F b) {
    return v4f(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

static Vec4F v4f_mul(Vec4F a, Vec4F b) {
    return v4f(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

static Vec4F v4f_div(Vec4F a, Vec4F b) {
    return v4f(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
}

static float v4f_dot(Vec4F a, Vec4F b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

static float v4f_len_squared(Vec4F v) {
    return v4f_dot(v, v);
}

static float v4f_len(Vec4F v) {
    return sqrtf(v4f_dot(v, v));
}

static Vec4F v4f_normalize(Vec4F v) {
    return v4f_scale(v, 1.0f/v4f_len(v));
}

static Vec4F v4f_lerp(Vec4F a, Vec4F b, float t) {
    return v4f_add(v4f_scale(a, 1 - t), v4f_scale(b, t));
}

static Vec4F v4f_transform(Vec4F v, Mat4x4F transform) {
    Vec4F result = {0};
    for (size_t i = 0; i < 4; i++) {
        result.v[i] = v.x * transform.m[0][i]
            + v.y * transform.m[1][i]
            + v.z * transform.m[2][i]
            + v.w * transform.m[3][i];
    }
    return result;
}

// F64
static Vec4D v4d_id() {
    return v4d(1, 1, 1, 1);
}

static Vec4D v4d_scale(Vec4D a, double s) {
    return v4d(a.x * s, a.y * s, a.z * s, a.w * s);
}

static Vec4D v4d_add(Vec4D a, Vec4D b) {
    return v4d(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

static Vec4D v4d_sub(Vec4D a, Vec4D b) {
    return v4d(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

static Vec4D v4d_mul(Vec4D a, Vec4D b) {
    return v4d(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

static Vec4D v4d_div(Vec4D a, Vec4D b) {
    return v4d(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
}

static double v4d_dot(Vec4D a, Vec4D b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

static double v4d_len_squared(Vec4D v) {
    return v4d_dot(v, v);
}

static double v4d_len(Vec4D v) {
    return sqrt(v4d_dot(v, v));
}

static Vec4D v4d_normalize(Vec4D v) {
    return v4d_scale(v, 1.0f/v4d_len(v));
}

static Vec4D v4d_lerp(Vec4D a, Vec4D b, double t) {
    return v4d_add(v4d_scale(a, 1 - t), v4d_scale(b, t));
}

static Vec4D v4d_transform(Vec4D v, Mat4x4D transform) {
    Vec4D result = {0};
    for (size_t i = 0; i < 4; i++) {
        result.v[i] = v.x * transform.m[0][i]
             + v.y * transform.m[1][i]
             + v.z * transform.m[2][i]
             + v.w * transform.m[3][i];
    }
    return result;
}

// Matrix Operations
// 2x2
float m2x2f_determinant(Mat2x2F m) {
    return (m.m[0][0] * m.m[1][1]) - (m.m[1][0] * m.m[0][1]);
}

static double m2x2d_determinant(Mat2x2D m) {
    return (m.m[0][0] * m.m[1][1]) - (m.m[1][0] * m.m[0][1]);
}

// 3x3
float m3x3f_determinant(Mat3x3F m) {
    return m.m[0][0] * ((m.m[1][1] * m.m[2][2]) - (m.m[2][1] * m.m[1][2]))
         - m.m[0][1] * ((m.m[1][0] * m.m[2][2]) - (m.m[2][0] * m.m[1][2]))
         + m.m[0][2] * ((m.m[1][0] * m.m[2][1]) - (m.m[2][0] * m.m[1][1]));
}

static double m3x3d_determinant(Mat3x3D m) {
    return m.m[0][0] * ((m.m[1][1] * m.m[2][2]) - (m.m[2][1] * m.m[1][2]))
         - m.m[0][1] * ((m.m[1][0] * m.m[2][2]) - (m.m[2][0] * m.m[1][2]))
         + m.m[0][2] * ((m.m[1][0] * m.m[2][1]) - (m.m[2][0] * m.m[1][1]));
}

// 4x4
static Mat4x4F m4x4f_scale(Mat4x4F a, float s) {
    for (size_t i = 0; i < 4; i++) {
        for (size_t j = 0; j < 4; j++) {
            a.m[i][j] *= s;
        }
    }
    return a;
}

// TODO: check if the loop ordering is best for the caching
static Mat4x4F m4x4f_mul(Mat4x4F a, Mat4x4F b) {
    Mat4x4F result = {0};

    for (size_t j = 0; j < 4; j++) {
        for (size_t i = 0; i < 4; i++) {
            result.m[j][i] =
                a.m[0][i] * b.m[j][0]
              + a.m[1][i] * b.m[j][1]
              + a.m[2][i] * b.m[j][2]
              + a.m[3][i] * b.m[j][3];
        }
    }
    return result;
}

static Mat4x4F m4x4f_transpose(Mat4x4F m) {
    Mat4x4F result = {0};
    for (size_t i = 0; i < 4; i++) {
        for (size_t j = 0; j < 4; j++) {
            result.m[i][j] = m.m[j][i];
        }
    }

    return result;
}

float m4x4f_determinant(Mat4x4F m) {
    return
        m.m[0][0] * (
            m.m[1][1]*m.m[2][2]*m.m[3][3] 
          + m.m[1][2]*m.m[2][3]*m.m[3][1] 
          + m.m[1][3]*m.m[2][1]*m.m[3][2] 
          - m.m[1][3]*m.m[2][2]*m.m[3][1] 
          - m.m[1][2]*m.m[2][1]*m.m[3][3] 
          - m.m[1][1]*m.m[2][3]*m.m[3][2] 
        )
      - m.m[1][0] * (
            m.m[0][1]*m.m[2][2]*m.m[3][3] 
          + m.m[0][2]*m.m[2][3]*m.m[3][1] 
          + m.m[0][3]*m.m[2][1]*m.m[3][2] 
          - m.m[0][3]*m.m[2][2]*m.m[3][1] 
          - m.m[0][2]*m.m[2][1]*m.m[3][3] 
          - m.m[0][1]*m.m[2][3]*m.m[3][2]
        )
      + m.m[2][0] * (
            m.m[0][1]*m.m[1][2]*m.m[3][3]
          + m.m[0][2]*m.m[1][3]*m.m[3][1]
          + m.m[0][3]*m.m[1][1]*m.m[3][2]
          - m.m[0][3]*m.m[1][2]*m.m[3][1]
          - m.m[0][2]*m.m[1][1]*m.m[3][3]
          - m.m[0][1]*m.m[1][3]*m.m[3][2]
        )
      - m.m[3][0] * (
            m.m[0][1]*m.m[1][2]*m.m[2][3]
          + m.m[0][2]*m.m[1][3]*m.m[2][1]
          + m.m[0][3]*m.m[1][1]*m.m[2][2]
          - m.m[0][3]*m.m[1][2]*m.m[2][1]
          - m.m[0][2]*m.m[1][1]*m.m[2][3]
          - m.m[0][1]*m.m[1][3]*m.m[2][2]
        );
}


static double m4x4d_determinant(Mat4x4D m) {
    return
        m.m[0][0] * (
            m.m[1][1]*m.m[2][2]*m.m[3][3]
          + m.m[1][2]*m.m[2][3]*m.m[3][1]
          + m.m[1][3]*m.m[2][1]*m.m[3][2]
          - m.m[1][3]*m.m[2][2]*m.m[3][1]
          - m.m[1][2]*m.m[2][1]*m.m[3][3]
          - m.m[1][1]*m.m[2][3]*m.m[3][2]
        )
      - m.m[1][0] * (
            m.m[0][1]*m.m[2][2]*m.m[3][3]
          + m.m[0][2]*m.m[2][3]*m.m[3][1]
          + m.m[0][3]*m.m[2][1]*m.m[3][2]
          - m.m[0][3]*m.m[2][2]*m.m[3][1]
          - m.m[0][2]*m.m[2][1]*m.m[3][3]
          - m.m[0][1]*m.m[2][3]*m.m[3][2]
        )
      + m.m[2][0] * (
            m.m[0][1]*m.m[1][2]*m.m[3][3]
          + m.m[0][2]*m.m[1][3]*m.m[3][1]
          + m.m[0][3]*m.m[1][1]*m.m[3][2]
          - m.m[0][3]*m.m[1][2]*m.m[3][1]
          - m.m[0][2]*m.m[1][1]*m.m[3][3]
          - m.m[0][1]*m.m[1][3]*m.m[3][2]
        )
      - m.m[3][0] * (
            m.m[0][1]*m.m[1][2]*m.m[2][3]
          + m.m[0][2]*m.m[1][3]*m.m[2][1]
          + m.m[0][3]*m.m[1][1]*m.m[2][2]
          - m.m[0][3]*m.m[1][2]*m.m[2][1]
          - m.m[0][2]*m.m[1][1]*m.m[2][3]
          - m.m[0][1]*m.m[1][3]*m.m[2][2]
        );
}

#endif // ifndef MIGI_MATH_H
