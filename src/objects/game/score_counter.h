#pragma once

#include "../../libs/animation.h"

class ScoreCounter {
private:
    int score;
    bool is_2p;
    TextStretchAnimation* stretch;

public:
    ScoreCounter(int score, bool is_2p);
    void update_count(int score);
    void update(double current_ms);
    void draw(float y);

};
