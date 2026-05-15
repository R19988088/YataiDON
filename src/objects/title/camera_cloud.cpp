#include "camera_cloud.h"
#include "../../libs/texture.h"

CameraCloud::CameraCloud() {
    fade_in = (FadeAnimation*)tex.get_animation(32);
    fade_in->start();
    text_fade = (FadeAnimation*)tex.get_animation(33);
    text_fade->start();
    move_up = (MoveAnimation*)tex.get_animation(34);
    move_up->start();
    breathing = (TextureResizeAnimation*)tex.get_animation(35);
    breathing->start();
}

void CameraCloud::update(double current_ms) {
    fade_in->update(current_ms);
    text_fade->update(current_ms);
    move_up->update(current_ms);
    breathing->update(current_ms);
}

void CameraCloud::draw() {
    tex.draw_texture(CAMERA::CAMERA_CLOUD, {.scale=(float)breathing->attribute, .center=true, .y=(float)move_up->attribute, .fade=fade_in->attribute});
    tex.draw_texture(CAMERA::CAMERA_CLOUD_TEXT_1, {.scale=(float)breathing->attribute, .center=true, .y=(float)move_up->attribute, .fade=std::min(fade_in->attribute, 1 - text_fade->attribute)});
    tex.draw_texture(CAMERA::CAMERA_CLOUD_TEXT_2, {.scale=(float)breathing->attribute, .center=true, .y=(float)move_up->attribute, .fade=std::min(fade_in->attribute, text_fade->attribute)});
}
