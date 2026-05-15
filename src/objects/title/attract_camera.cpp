#include "attract_camera.h"
#include "../../libs/texture.h"
#include "../../libs/audio.h"

AttractCamera::AttractCamera() {
    start_ms = get_current_ms();
    camera = WebCamera();
    camera.open();
    audio.play_sound("camera", VolumePreset::ATTRACT_MODE);

    live_icon_texture_change = (TextureChangeAnimation*)tex.get_animation(36);
    live_icon_texture_change->start();
}

void AttractCamera::update(double current_ms) {
    camera.update();
    live_icon_texture_change->update(current_ms);
    finished = (current_ms - start_ms >= 30000);
}

void AttractCamera::draw() {
    tex.draw_texture(CAMERA::BACKGROUND);
    if (camera.is_ready()) {
        ray::Rectangle src = {0, 0, (float)camera.width(), (float)camera.height()};
        ray::Rectangle dst = {332, 119, 630 * tex.screen_scale, 470 * tex.screen_scale};
        ray::DrawTexturePro(camera.get_texture(), src, dst, ray::Vector2(0, 0), 0, ray::WHITE);
    }
    tex.draw_texture(CAMERA::LIVE_ICON, {.frame=(int)live_icon_texture_change->attribute});
    tex.draw_texture(CAMERA::LIVE_TEXT);
}

bool AttractCamera::is_finished() {
    return finished;
}
