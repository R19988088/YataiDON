#pragma once

#include "../../libs/texture.h"
#include "../../libs/ray.h"

class Chara3D {
private:
    ray::Model model;

    ray::ModelAnimation* anims;
    int anim_count = 0;
    int anim_frame = 0;
    int anim_index = 20;

    float bpm = 100;
    double last_frame_ms = 0;

    float scale = 750.0f;
    float rot_x = 180.0f;
    float rot_y = 20.0f;
    float rot_z = 0.0f;

    int prev_anim_idx = 0;
    bool is_looping = true;

    ray::Shader outline_shader;
    float outline_thickness = 1.0f;

    void set_texture(fs::path& texture_path, int material_index);
public:
    Chara3D(std::string& model_name);

    ~Chara3D();

    void set_body_texture(fs::path& texture_path);
    void set_face_rim_texture(fs::path& texture_path);
    void set_face_texture(fs::path& texture_path);

    void set_body_colors(ray::Color body, ray::Color rim);
    void set_face_colors(ray::Color face, ray::Color rim);

    void set_bpm(float bpm);
    void set_anim(int idx);
    int  get_anim_count() const;
    std::string get_anim_name(int idx);

    void update(double current_ms);

    void draw(float x, float y);
};
