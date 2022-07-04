#include "my_math.h"

#include <intrin.h>
#include <math.h>

Vec2 Vec2::operator+(Vec2 other) const
{
    return Vec2{
        x + other.x,
        y + other.y};
}

Vec2 Vec2::operator+(float scalar) const
{
    return Vec2{
        x + scalar,
        y + scalar};
}

Vec2 &Vec2::operator+=(Vec2 other)
{
    x += other.x;
    y += other.y;

    return *this;
}

Vec2 Vec2::operator*(float scalar) const 
{
    return Vec2{
        x * scalar,
        y * scalar};
}

Vec2 &Vec2::operator*=(float scalar)
{
    x *= scalar;
    y *= scalar;

    return *this;
}

Vec2 Vec2::operator*(Vec2 other) const
{
    return Vec2{
        x * other.x,
        y * other.y};
}

Vec2 Vec2::operator-(const Vec2 other) const
{
    return Vec2{
        x - other.x,
        y - other.y};
}

Vec2 &Vec2::operator-=(Vec2 other)
{
    x -= other.x;
    y -= other.y;

    return *this;
}

Vec2 Vec2::operator/(float scalar) const 
{
    return Vec2{
        x / scalar,
        y / scalar};
}

Vec2 Vec2::operator/(Vec2 other) const 
{
    return Vec2{
        x / other.x,
        y / other.y};
}

bool Vec2::operator==(Vec2 other)
{
    return x == other.x && y == other.y;
}

float length(Vec2 v)
{
    return sqrt((v.x * v.x) + (v.y * v.y));
}

Vec2 normalize(Vec2 v)
{
    float vecLenght = length(v);
    if (vecLenght <= 0)
        return Vec2{0.0f, 0.0f};

    return Vec2{
        v.x / vecLenght,
        v.y / vecLenght};
}

Vec2 lerp(Vec2 p0, Vec2 p1, float t)
{
    Vec2 result = p0 * (1.0f - t) + p1 * t;
    return result;
}

Vec2 quadratic_bezier(Vec2 p0, Vec2 p1, Vec2 m, float t)
{
    Vec2 p0ToMiddle = lerp(p0, m, t);
    Vec2 middleToP1 = lerp(m, p1, t);
    Vec2 middlePoint = lerp(p0ToMiddle, middleToP1, t);
    return middlePoint;
}

Vec2 cubic_bezier(Vec2 p0, Vec2 m0, Vec2 p1, Vec2 m1, float t)
{
    Vec2 p0ToM0 = lerp(p0, m0, t);
    Vec2 m0ToM1 = lerp(m0, m1, t);
    Vec2 m1ToP1 = lerp(m1, p1, t);

    Vec2 p0ToM0_to_m0ToM1 = lerp(p0ToM0, m0ToM1, t);
    Vec2 m0ToM1_to_m1ToP1 = lerp(m0ToM1, m1ToP1, t);

    Vec2 middlePoint = lerp(p0ToM0_to_m0ToM1, m0ToM1_to_m1ToP1, t);

    return middlePoint;
}

Vec2 generate_point_on_cubic_bezier(
    Vec2 pointA,
    Vec2 pointAMiddle,
    Vec2 pointB,
    Vec2 pointBMiddle,
    float progressBetween)
{
    Vec2 result = cubic_bezier(
        pointA,
        pointA + (pointA - pointAMiddle),
        pointB,
        pointBMiddle,
        progressBetween);

    return result;
}

Vec2 normal_from_line_top(Vec2 a, Vec2 b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return normalize(Vec2{dy, -(dx)});
}

Vec2 normal_from_line_bottom(Vec2 a, Vec2 b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return normalize(Vec2{-(dy), dx});
}

Vec2 reflect_by_normal(Vec2 normal, Vec2 direction)
{
    /**
        * Taken from https://math.stackexchange.com/questions/13261/how-to-get-a-reflection-vector
        */
    normal = normalize(normal);
    return direction - normal * 2.0f * dot(direction, normal);
}

Vec2 rotate_by_direction(Vec2 point, Vec2 d)
{
    float angle = atan2f(d.y, d.x) + 3.14f / 2.0f;

    return {(point.x) * sinf(angle) - (point.y) * cosf(angle),
            (point.x) * cosf(angle) + (point.y) * sinf(angle)};
}

Vec2 sin_between_two_points(Vec2 a, Vec2 b, float t, float speedup)
{
    Vec2 direction = b - a;

    // [0:1] range
    float progress = sinf(t * speedup);

    Vec2 pos = a + direction * progress;
    return pos;
}

float dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

float length_squared(Vec2 v)
{
    return (v.x * v.x) + (v.y * v.y);
}

float distance_point_from_line(Vec2 A, Vec2 B, Vec2 P)
{
    /**
        * A Dot Product give projection * length of Vector being
        * projected on
        */

    Vec2 AB = B - A;
    Vec2 AP = P - A;
    float ABProjectionTimeABLenght = dot(B - A, B - A);
    float APProjectionTimesABLength = dot(P - A, B - A);

    float t = APProjectionTimesABLength / ABProjectionTimeABLenght;
    Vec2 C = AB * t;

    return 0.0f;
}

bool line_intersection(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Vec2 *collisionPoint)
{
    CAKEZ_ASSERT(collisionPoint, "No collision point supplied!");

    // Vector from a to b
    Vec2 r = (b - a);

    // Vector from c to d
    Vec2 s = (d - c);

    // Dot Product with (r dot s)
    float d1 = r.x * s.y - r.y * s.x;
    float u = ((c.x - a.x) * r.y - (c.y - a.y) * r.x) / d1;
    float t = ((c.x - a.x) * s.y - (c.y - a.y) * s.x) / d1;

    // If they intersect, return where
    if (0 <= u && u <= 1 && 0 <= t && t <= 1)
    {
        *collisionPoint = a + r * t;
        return true;
    }
    return false;
}

float &Vec3::operator[](int index)
{
    return values[index];
}

Vec3 Vec3::operator+(Vec3 other)
{
    return Vec3{
        x + other.x,
        y + other.y,
        z + other.z};
}

Vec3 Vec3::operator*(float value)
{
    return Vec3{
        x * value,
        y * value,
        z * value};
}

Vec3 Vec3::operator-(Vec3 other)
{
    return Vec3{
        x - other.x,
        y - other.y,
        z - other.z};
}

float length(Vec3 v)
{
    return sqrt((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
}

Vec3 normalize(Vec3 v)
{
    float vecLenght = length(v);
    if (vecLenght <= 0)
        return Vec3{0.0f, 0.0f, 0.0f};

    return Vec3{
        v.x / vecLenght,
        v.y / vecLenght,
        v.z / vecLenght};
}

Vec3 cross(Vec3 a, Vec3 b)
{
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

float dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

bool Vec4::operator==(Vec4 other)
{
    return x == other.x && y == other.y && z == other.z && w == other.w;
}

Vec4 Vec4::operator+(Vec4 other)
{
    return {
        x + other.x,
        y + other.y,
        z + other.z,
        w + other.w};
}

Vec4 Vec4::operator*(float value)
{
    return {
        x * value,
        y * value,
        z * value,
        w * value};
}

float &Vec4::operator[](int index)
{
    return values[index];
}

Vec4 &Mat4::operator[](int col)
{
    return cols[col];
}

Mat4 Mat4::operator*(Mat4 other)
{
    Mat4 result = {};

    // TODO: think about how to do this in a for loop

    // for (int i = 0; i < 4; i++)
    // {
    //     for (int j = 0; j < 4; j++)
    //     {
    //         result[i][j] += cols[j][i] * other[i][j];
    //     }
    // }
    result.ax = ax * other.ax + bx * other.ay + cx * other.az + dx * other.aw;
    result.ay = ay * other.ax + by * other.ay + cy * other.az + dy * other.aw;
    result.az = az * other.ax + bz * other.ay + cz * other.az + dz * other.aw;
    result.aw = aw * other.ax + bw * other.ay + cw * other.az + dw * other.aw;

    result.bx = ax * other.bx + bx * other.by + cx * other.bz + dx * other.bw;
    result.by = ay * other.bx + by * other.by + cy * other.bz + dy * other.bw;
    result.bz = az * other.bx + bz * other.by + cz * other.bz + dz * other.bw;
    result.bw = aw * other.bx + bw * other.by + cw * other.bz + dw * other.bw;

    result.cx = ax * other.cx + bx * other.cy + cx * other.cz + dx * other.cw;
    result.cy = ay * other.cx + by * other.cy + cy * other.cz + dy * other.cw;
    result.cz = az * other.cx + bz * other.cy + cz * other.cz + dz * other.cw;
    result.cw = aw * other.cx + bw * other.cy + cw * other.cz + dw * other.cw;

    result.dx = ax * other.dx + bx * other.dy + cx * other.dz + dx * other.dw;
    result.dy = ay * other.dx + by * other.dy + cy * other.dz + dy * other.dw;
    result.dz = az * other.dx + bz * other.dy + cz * other.dz + dz * other.dw;
    result.dw = aw * other.dx + bw * other.dy + cw * other.dz + dw * other.dw;

    return result;
}

Mat4 create_mat4(float initValue)
{
    Mat4 mat = {};

    mat[0][0] = initValue;
    mat[1][1] = initValue;
    mat[2][2] = initValue;
    mat[3][3] = initValue;

    return mat;
}

Mat4 translate(Mat4 m, Vec3 v)
{
    Mat4 result = create_mat4(1.0f);
    result[3] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2] + m[3];
    return result;
}

Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up)
{
    Vec3 zaxis = normalize(target - eye);
    Vec3 xaxis = normalize(cross(zaxis, up));
    Vec3 yaxis = cross(xaxis, zaxis);

    Mat4 result = create_mat4(1.0f);
    result[0][0] = xaxis.x;
    result[1][0] = xaxis.y;
    result[2][0] = xaxis.z;
    result[0][1] = yaxis.x;
    result[1][1] = yaxis.y;
    result[2][1] = yaxis.z;
    result[0][2] = -zaxis.x;
    result[1][2] = -zaxis.y;
    result[2][2] = -zaxis.z;
    result[3][0] = -dot(xaxis, eye);
    result[3][1] = -dot(yaxis, eye);
    result[3][2] = dot(zaxis, eye);

    return result;
}

Rect Rect::operator*(float scalar)
{
    return {
        pos.x * scalar,
        pos.y * scalar,
        size.x * scalar,
        size.y * scalar};
}

Rect &Rect::operator*=(float scalar)
{
    pos.x *= scalar;
    pos.y *= scalar;
    size.x *= scalar;
    size.y *= scalar;

    return *this;
}

bool point_in_rect(Vec2 point, Rect rect)
{
    return (point.x >= rect.left &&
            point.x <= rect.left + rect.sizeX &&
            point.y >= rect.top &&
            point.y <= rect.top + rect.sizeY);
}

bool rect_collision(Rect a, Rect b)
{
    return (point_in_rect(Vec2{a.left, a.top}, b) ||
            point_in_rect(Vec2{a.left + a.sizeX, a.top}, b) ||
            point_in_rect(Vec2{a.left, a.top + a.sizeY}, b) ||
            point_in_rect(Vec2{a.left + a.sizeX, a.top + a.sizeY}, b));
}

bool point_in_circle(Vec2 point, Vec2 origin, float r)
{
    Vec2 pointToOrigen = origin - point;
    float lengthSquared = length_squared(pointToOrigen);
    return lengthSquared <= r * r;
}

bool circle_collision(Circle *circleA, Circle *circleB)
{
    return circle_collision(circleA->radius, circleA->origin, circleB->radius, circleB->origin);
}

bool circle_collision(Circle circleA, Circle circleB)
{
    return circle_collision(&circleA, &circleB);
}

bool circle_collision(float rA, Vec2 originA, float rB, Vec2 originB)
{
    float r12Squared = (rA + rB) * (rA + rB);
    Vec2 origin1ToOrigin2 = originA - originB;

    float squaredLenght = length_squared(origin1ToOrigin2);

    if (squaredLenght > r12Squared)
    {
        return false;
    }

    return true;
}

bool circle_collision(float rA, Vec2 originA, Circle circle)
{
    return circle_collision(rA, originA, circle.radius, circle.origin);
}

bool within_range(float a, float b, float point)
{
    bool result = false;
    if (a <= point && point <= b)
        result = true;

    return result;
}

float sqrt(float value)
{
    return _mm_cvtss_f32(_mm_sqrt_ps(_mm_set_ss(value)));
}

float clamp(float value, float min, float max)
{
    if (value > max)
    {
        value = max;
    }
    else if (value < min)
    {
        value = min;
    }

    return value;
}