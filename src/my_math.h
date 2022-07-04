#pragma once

struct Vec2
{
    float x;
    float y;

    Vec2 operator+(Vec2 other) const;
    Vec2 operator+(float scalar) const;
    Vec2 &operator+=(Vec2 other);

    Vec2 operator*(float scalar) const;
    Vec2 &operator*=(float scalar);
    Vec2 operator*(Vec2 other) const;

    Vec2 operator-(Vec2 other) const;
    Vec2 &operator-=(Vec2 other);

    Vec2 operator/(float scalar) const;
    Vec2 operator/(Vec2 other) const;

    bool operator==(Vec2 other);
};

struct IVec2
{
    union
    {
        s32 values[2];
        struct
        {
            s32 x;
            s32 y;
        };
        struct
        {
            s32 xy[2];
        };
    };
};

float length(Vec2 v);
Vec2 normalize(Vec2 v);
Vec2 lerp(Vec2 p0, Vec2 p1, float t);
Vec2 quadratic_bezier(Vec2 p0, Vec2 p1, Vec2 m, float t);
Vec2 cubic_bezier(Vec2 p0, Vec2 m1, Vec2 p1, Vec2 m2, float t);
/**
* @param progressBetween float value in [0; 1]
*/
Vec2 generate_pos32_on_cubic_bezier(Vec2 pos32A, Vec2 pos32AMiddle,
                                    Vec2 pos32B, Vec2 pos32BMiddle, float progressBetween);

Vec2 normal_from_line_top(Vec2 a, Vec2 b);
Vec2 normal_from_line_bottom(Vec2 a, Vec2 b);
Vec2 reflect_by_normal(Vec2 normal, Vec2 direction);
Vec2 rotate_by_direction(Vec2 pos32, Vec2 d);
Vec2 sin_between_two_pos32s(Vec2 a, Vec2 b, float t, float speedup = 1.0f);

float dot(Vec2 a, Vec2 b);
float length_squared(Vec2 v);
float distance_pos32_from_line(Vec2 A, Vec2 B, Vec2 P);
bool line_s32ersection(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Vec2 *collisionPos32);

struct Vec3
{
    union
    {
        float values[3];
        struct
        {
            float x;
            float y;
            float z;
        };
        struct
        {
            float xy[2];
            float z;
        };

        struct
        {
            float xyz[3];
        };
    };

    float &operator[](s32 index);
    Vec3 operator+(Vec3 other);
    Vec3 operator*(float value);
    Vec3 operator-(Vec3 other);
};

struct IVec3
{
    union
    {
        s32 values[3];
        struct
        {
            s32 x;
            s32 y;
            s32 z;
        };
        struct
        {
            s32 xy[2];
            s32 z;
        };

        struct
        {
            s32 xyz[3];
        };
    };
};

float length(Vec3 v);
Vec3 normalize(Vec3 v);
Vec3 cross(Vec3 a, Vec3 b);
float dot(Vec3 a, Vec3 b);

struct Vec4
{
    union
    {
        float values[4];
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };

        struct
        {
            float r;
            float g;
            float b;
            float a;
        };

        struct
        {
            float xy[2];
            float z;
            float w;
        };

        struct
        {
            float xyz[3];
            float w;
        };
    };

    bool operator==(Vec4 other);
    Vec4 operator+(Vec4 other);
    Vec4 operator*(float vlaue);
    float &operator[](s32 index);
};

struct IVec4
{
    union
    {
        s32 values[4];
        struct
        {
            s32 x;
            s32 y;
            s32 z;
            s32 w;
        };
        union
        {
            struct
            {
                s32 xy[2];
                s32 z;
                s32 w;
            };
            struct
            {
                s32 xy[2];
                s32 zw[2];
            };
        };

        struct
        {
            s32 xyz[3];
            s32 w;
        };
    };
};

Vec4 create_vec4(float *buffer);

struct Quat
{
    union
    {
        float values[4];
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };
    };
};

Quat create_quat(float *buffer);

struct Mat3
{
    union
    {
        float values[9];
        Vec3 cols[3];
        struct
        {
            float ax;
            float ay;
            float az;

            float bx;
            float by;
            float bz;

            float cx;
            float cy;
            float cz;
        };
    };
};

struct Mat4
{
    union
    {
        float values[16];
        Vec4 cols[4];
        struct
        {
            float ax;
            float ay;
            float az;
            float aw;

            float bx;
            float by;
            float bz;
            float bw;

            float cx;
            float cy;
            float cz;
            float cw;

            float dx;
            float dy;
            float dz;
            float dw;
        };
    };

    Vec4 &operator[](s32 col);
    Mat4 operator*(Mat4 other);
};

Mat4 create_mat4(float initValue);
Mat4 create_mat4(float *buffer);
Mat4 translate(Mat4 m, Vec3 v);
Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up);

struct Rect
{
    union
    {
        struct
        {
            Vec2 pos;
            Vec2 size;
        };

        // Note: To make comparisons easier, order matters
        struct
        {
            float left;
            float top;
            float sizeX;
            float sizeY;
        };

        struct
        {
            float xy[2];
            float z;
            float w;
        };

        struct
        {
            float xyz[3];
            float w;
        };
    };

    Rect operator*(float scalar);
    Rect &operator*=(float scala);
};

bool pos32_in_rect(Vec2 pos32, Rect rect);
bool rect_collision(Rect a, Rect b);

struct Circle
{
    float radius;
    Vec2 origin;
};

//TODO: Implement later
struct CollisionInformation
{
    bool colided;
    // Entity *collider;
    float penetrationDepth;
};

bool pos32_in_circle(Vec2 pos32, Vec2 origin, float r);
bool circle_collision(Circle *circleA, Circle *circleB);
bool circle_collision(Circle circleA, Circle circleB);
bool circle_collision(float rA, Vec2 originA, float rB, Vec2 originB);
bool circle_collision(float rA, Vec2 originA, Circle circle);

bool within_range(float a, float b, float pos32);

float sqrt(float value);

float clamp(float value, float min, float max);

