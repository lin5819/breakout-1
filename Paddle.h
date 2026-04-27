#ifndef PADDLE_H
#define PADDLE_H

#include "raylib.h"

class Paddle {
private:
    Rectangle rect;
    float speed; // 新增成员变量
public:
    // 修改构造函数，增加 speed 参数
    Paddle(float x, float y, float w, float h, float s); 
    void Draw(Rectangle paddleRect);
    void MoveLeft();
    void MoveRight();
    Rectangle GetRect() { return rect; }
    void SetRect(Rectangle a) {rect = a ;}
};

#endif