#pragma once

#include "../../libs/animation.h"

enum class BanaAdState {
    BANA_OPTIONS,
    TAP_TO_READER,
    REWARDS,
    CARD_OR_PHONE,
    GET
};

class BanaAdvertisement {
private:
    BanaAdState current_state;

    FadeAnimation* fade_in;
    TextureResizeAnimation* resize_1;
    TextureResizeAnimation* resize_2;

    TextureResizeAnimation* bana_rotate;
    MoveAnimation* bana_move;
    FadeAnimation* touch_fade;
    MoveAnimation* touch_move_up;
    MoveAnimation* touch_move_down;
    TextureResizeAnimation* touch_resize;
    FadeAnimation* touch_fade_out;

    MoveAnimation* donchan_move;

    TextureResizeAnimation* get_text_scale_down;
    TextureResizeAnimation* get_text_scale_up;
    TextureResizeAnimation* get_text_scale_down_2;
    FadeAnimation* get_text_fade_in;
    FadeAnimation* get_fade_out;
    MoveAnimation* get_chara_bounce_up;
    MoveAnimation* get_chara_bounce_down;
    MoveAnimation* get_chara_1_bounce_up;
    MoveAnimation* get_chara_1_bounce_down;
    double chara_1_timer = -1.0;

    int steps_cursor = 0;
    int loop_counter = 0;
    double timer;
    double rewards_angle = 0.0;
    double rewards_last_ms = -1.0;
public:
    BanaAdvertisement();

    void update(double current_ms);

    void draw(float x, float y);
};
