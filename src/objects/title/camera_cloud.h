#pragma once

#include "../../libs/animation.h"

class CameraCloud {
private:
    FadeAnimation* fade_in;
    FadeAnimation* text_fade;
    MoveAnimation* move_up;
    TextureResizeAnimation* breathing;
public:
    CameraCloud();

    void update(double current_ms);

    void draw();
};
