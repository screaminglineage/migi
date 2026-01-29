#include "migi_math.h"

void test_determinant() {
    {
        Mat2x2F m;
        m = (Mat2x2F){
            5, 6,
            8, 9
        };
        assert(isclose(m2x2f_determinant(m), -3.0));

        m = (Mat2x2F){
            1, 7,
            9, 8
        };
        assert(isclose(m2x2f_determinant(m), -55.0));

        assert(isclose(m2x2f_determinant(m2x2f(1)), 1));
    }
    {
        Mat3x3F m;
        m = (Mat3x3F){
            1, 2, 3,
            4, 5, 6,
            7, 8, 9
        };
        assert(isclose(m3x3f_determinant(m), 0.0));

        m = (Mat3x3F){
            3, 5, 2,
            1, 4, 7,
            9, 6, 8
        };
        assert(isclose(m3x3f_determinant(m), 185.0));

        assert(isclose(m3x3f_determinant(m3x3f(1)), 1));
    }
    {
        Mat4x4F m;
        m = (Mat4x4F){
            1,  2,  3, 4,
            5,  6,  7, 8,
            9, 10, 11, 12
        };
        assert(isclose(m4x4f_determinant(m), 0.0));

        m = (Mat4x4F){
            1,  0,  4, -6,
            2,  5,  0,  3,
            -1, 2,  3,  5,
            2,  1, -2,  3
        };
        assert(isclose(m4x4f_determinant(m), 318.0));

        assert(isclose(m4x4f_determinant(m4x4f(1)), 1));
    }
}

void test_transform() {
    {
        Vec3D v = {-1, 2, 3};
        Mat3x3D t = {
            1, 0, 0,
            0, 1, 0,
            0, 0, 0
        };
        assert(v3_isclose(v3d_transform(v, t), v3f(-1, 2, 0)));
    }

    // TODO: assert these (change the matrix)
    v2d_transform(v2d(1, 2), m2x2d(1));
    v3d_transform(v3d(1, 2, 3), m3x3d(1));
    v4d_transform(v4d(1, 2, 3, 4), m4x4d(1));

    v2f_transform(v2f(1, 2), m2x2f(1));
    v3f_transform(v3f(1, 2, 3), m3x3f(1));
    v4f_transform(v4f(1, 2, 3, 4), m4x4f(1));

}

void test_mul() {
    {
        Mat2x2F id = m2x2f(1);
        Mat2x2F m1 = {
            1, 2,
            3, 4
        };
        Mat2x2F m2 = m2x2f_mul(m1, id);
        assert(m2x2_isclose(m2, m1));
    }
    {
        Mat3x3F id = m3x3f(1);
        Mat3x3F m1 = {
            1,  2,  3,
            4,  5,  6,
            7,  8,  9,
        };
        Mat3x3F m2 = m3x3f_mul(m1, id);
        assert(m3x3_isclose(m2, m1));
    }
    {
        Mat4x4F id = m4x4f(1);
        Mat4x4F m1 = {
            1,  2,  3,  4,
            5,  6,  7,  8,
            9, 10, 11, 12
        };
        Mat4x4F m2 = m4x4f_mul(m1, id);
        assert(m4x4_isclose(m2, m1));
    }
}

int main() {
    test_determinant();
    test_transform();
    test_mul();



    printf("\nExiting Successfully\n");
    return 0;
}
