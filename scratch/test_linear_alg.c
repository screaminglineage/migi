#include "migi.h"
#include "linear_algebra.h"




void test_determinant() {
    {
        Mat2x2F m;
        m = (Mat2x2F){
            5, 6,
            8, 9
        };
        assert(isclose(mat_determinant(m), -3.0));

        m = (Mat2x2F){
            1, 7,
            9, 8
        };
        assert(isclose(mat_determinant(m), -55.0));

        assert(isclose(mat_determinant(m2x2f(1)), 1));
    }
    {
        Mat3x3F m;
        m = (Mat3x3F){
            1, 2, 3,
            4, 5, 6,
            7, 8, 9
        };
        assert(isclose(mat_determinant(m), 0.0));

        m = (Mat3x3F){
            3, 5, 2,
            1, 4, 7,
            9, 6, 8
        };
        assert(isclose(mat_determinant(m), 185.0));

        assert(isclose(mat_determinant(m3x3f(1)), 1));
    }
    {
        Mat4x4F m;
        m = (Mat4x4F){
            1,  2,  3, 4,
            5,  6,  7, 8,
            9, 10, 11, 12
        };
        assert(isclose(mat_determinant(m), 0.0));

        m = (Mat4x4F){
            1,  0,  4, -6,
            2,  5,  0,  3,
            -1, 2,  3,  5,
            2,  1, -2,  3
        };
        assert(isclose(mat_determinant(m), 318.0));

        assert(isclose(mat_determinant(m4x4f(1)), 1));
    }
    {
        Mat2x2D m;
        m = (Mat2x2D){
            5, 6,
            8, 9
        };
        assert(isclose(mat_determinant(m), -3.0));

        m = (Mat2x2D){
            1, 7,
            9, 8
        };
        assert(isclose(mat_determinant(m), -55.0));

        assert(isclose(mat_determinant(m2x2d(1)), 1));
    }
    {
        Mat3x3D m;
        m = (Mat3x3D){
            1, 2, 3,
            4, 5, 6,
            7, 8, 9
        };
        assert(isclose(mat_determinant(m), 0.0));

        m = (Mat3x3D){
            3, 5, 2,
            1, 4, 7,
            9, 6, 8
        };
        assert(isclose(mat_determinant(m), 185.0));

        assert(isclose(mat_determinant(m3x3d(1)), 1));
    }
    {
        Mat4x4D m;
        m = (Mat4x4D){
            1,  2,  3, 4,
            5,  6,  7, 8,
            9, 10, 11, 12
        };
        assert(isclose(mat_determinant(m), 0.0));

        m = (Mat4x4D){
            1,  0,  4, -6,
            2,  5,  0,  3,
            -1, 2,  3,  5,
            2,  1, -2,  3
        };
        assert(isclose(mat_determinant(m), 318.0));

        assert(isclose(mat_determinant(m4x4d(1)), 1));
    }
}

void test_transform() {
    {
        Vec2F v = {1, 2};
        Mat2x2F t = {
            2, 0,
            0, 3
        };
        assert(v2_isclose(vec_transform(v, t), v2f(2, 6)));
    }

    {
        Vec3F v = {1, 2, 3};
        Mat3x3F t = {
            1, 0, 0,
            0, 2, 0,
            0, 0, 3
        };
        assert(v3_isclose(vec_transform(v, t), v3f(1, 4, 9)));
    }

    {
        Vec4F v = {1, 2, 3, 4};
        Mat4x4F t = {
            1, 0, 0, 0,
            0, 2, 0, 0,
            0, 0, 3, 0,
            0, 0, 0, 4
        };
        assert(v4_isclose(vec_transform(v, t), v4f(1, 4, 9, 16)));
    }


    {
        Vec2D v = {1, 2};
        Mat2x2D t = {
            -1, 0,
            0, 5
        };
        assert(v2_isclose(vec_transform(v, t), v2d(-1, 10)));
    }

    {
        Vec3D v = {-1, 2, 3};
        Mat3x3D t = {
            1, 0, 0,
            0, 1, 0,
            0, 0, 0
        };
        assert(v3_isclose(vec_transform(v, t), v3d(-1, 2, 0)));
    }

    {
        Vec4D v = {1, 2, 3, 4};
        Mat4x4D t = {
            0.5, 0,   0,   0,
            0,   1.5, 0,   0,
            0,   0,   2.0, 0,
            0,   0,   0,   1.0
        };
        assert(v4_isclose(vec_transform(v, t), v4d(0.5, 3.0, 6.0, 4.0)));
    }
}


void test_mul() {
    {
        Mat2x2F id = m2x2f(1);
        Mat2x2F m1 = {
            1, 2,
            3, 4
        };
        Mat2x2F m2 = mat_mul(m1, id);
        assert(m2x2_isclose(m2, m1));
    }
    {
        Mat2x2D id = m2x2d(1);
        Mat2x2D m1 = {
            1, 2,
            3, 4
        };
        Mat2x2D m2 = mat_mul(m1, id);
        assert(m2x2_isclose(m2, m1));
    }
    {
        Mat3x3F id = m3x3f(1);
        Mat3x3F m1 = {
            1,  2,  3,
            4,  5,  6,
            7,  8,  9,
        };
        Mat3x3F m2 = mat_mul(m1, id);
        assert(m3x3_isclose(m2, m1));
    }
    {
        Mat3x3D id = m3x3d(1);
        Mat3x3D m1 = {
            1,  2,  3,
            4,  5,  6,
            7,  8,  9,
        };
        Mat3x3D m2 = mat_mul(m1, id);
        assert(m3x3_isclose(m2, m1));
    }
    {
        Mat4x4F id = m4x4f(1);
        Mat4x4F m1 = {
            1,  2,  3,  4,
            5,  6,  7,  8,
            9, 10, 11, 12
        };
        Mat4x4F m2 = mat_mul(m1, id);
        assert(m4x4_isclose(m2, m1));
    }
    {
        Mat4x4D id = m4x4d(1);
        Mat4x4D m1 = {
            1,  2,  3,  4,
            5,  6,  7,  8,
            9, 10, 11, 12
        };
        Mat4x4D m2 = mat_mul(m1, id);
        assert(m4x4_isclose(m2, m1));
    }
}

void test_generic_mul() {
    Temp tmp = arena_temp();
    // same shape
    {
        MatD m = {0};
        m.rows = 10;
        m.cols = 10;
        m.data = arena_push(tmp.arena, double, m.rows * m.cols);

        int c = 1;
        for (size_t i = 0; i < m.rows; i++) {
            for (size_t j = 0; j < m.cols; j++) {
                mat_at(m, i, j) = c++;
            }
        }

        MatD id = matd(tmp.arena, m.cols, m.cols, 1);
        MatD m1 = matn_mul(tmp.arena, m, id);

        for (size_t i = 0; i < m.rows; i++) {
            for (size_t j = 0; j < m.cols; j++) {
                assert(isclose(mat_at(m1, i, j), mat_at(m, i, j)));
            }
        }
    }

    // different shapes
    {
        MatD m1 = {
            .rows = 3,
            .cols = 3
        };
        m1.data = arena_push(tmp.arena, double, m1.rows * m1.cols);
        int c = 1;
        for (size_t i = 0; i < m1.rows; i++) {
            for (size_t j = 0; j < m1.cols; j++) {
                mat_at(m1, i, j) = c++;
            }
        }

        MatD m2 = {
            .rows = 3,
            .cols = 2
        };
        m2.data = arena_push(tmp.arena, double, m2.rows * m2.cols);
        c = 1;
        for (size_t i = 0; i < m2.rows; i++) {
            for (size_t j = 0; j < m2.cols; j++) {
                mat_at(m2, i, j) = c++;
            }
        }

        MatD m3 = matd_mul(tmp.arena, m1, m2);
        MatD m3_expected = {
            .data = (double[]){
                22.0, 28.0,
                49.0, 64.0,
                76.0, 100.0
            },
            .rows = 3,
            .cols = 2,
        };

        for (size_t i = 0; i < m3.rows; i++) {
            for (size_t j = 0; j < m3.cols; j++) {
                assert(isclose(mat_at(m3, i, j), mat_at(m3_expected, i, j)));
            }
        }
    }
    arena_temp_release(tmp);
}

int main() {
    test_determinant();
    test_transform();
    test_mul();
    test_generic_mul();

    Vec4F v = v4f(1, 2, 3, 4);
    mem_swap(v.xyz, v.yzw);
    printf("%f %f %f %f\n", v.x, v.y, v.z, v.w);

    Vec2F v1 = vec_scale(v2f(1, 2), 10.0);
    printf("%f %f\n", v1.x, v1.y);

    vec_len(v1);


    printf("\nExiting Successfully\n");
    return 0;
}
