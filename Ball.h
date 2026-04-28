#ifndef BALL_H
#define BALL_H

#include "raylib.h"

class Ball
{
private:
    Vector2 position;
    Vector2 speed;
    float radius;

public:
    static float maxspeedx;
    static float randspeedx;
    static float addspeedx;
    Ball(Vector2 pos, Vector2 sp, float r);
    void Move();
    void Draw();
    void BounceEdge(int screenWidth, int screenHeight);
    Vector2 GetPosition() { return position; }
    void Reset(Vector2 newPos, Vector2 newSpeed)
    {
        position = newPos;
        speed = newSpeed;
    }
    float GetRadius() { return radius; }
    // 为了改变速度，需要一个接口（或者直接在 Ball 内部处理反弹）
    void ReverseXSpeed() { speed.x = -speed.x; } // 新增：水平反弹
    void ReverseYSpeed() { speed.y = -speed.y; }
    // 如果需要修改速度向量本身，也可以添加 SetSpeed，这里用简单的反向

    void SetPosition(float x, float y);

    void Randspeedx();
    void Addspeedx();

    Vector2 GetSpeed() { return speed; }
};

#endif