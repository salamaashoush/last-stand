#pragma once
#include <string>
#include <unordered_map>
#include <expected>
#include <raylib.h>

namespace ls {

class AssetManager {
public:
    ~AssetManager() {
        for (auto& [_, tex] : textures_) UnloadTexture(tex);
        for (auto& [_, snd] : sounds_) UnloadSound(snd);
        for (auto& [_, fnt] : fonts_) UnloadFont(fnt);
        for (auto& [_, mus] : music_) UnloadMusicStream(mus);
    }

    std::expected<Texture2D, std::string> load_texture(const std::string& name, const std::string& path) {
        if (auto it = textures_.find(name); it != textures_.end())
            return it->second;
        if (!FileExists(path.c_str()))
            return std::unexpected("Texture not found: " + path);
        auto tex = LoadTexture(path.c_str());
        textures_[name] = tex;
        return tex;
    }

    std::expected<Sound, std::string> load_sound(const std::string& name, const std::string& path) {
        if (auto it = sounds_.find(name); it != sounds_.end())
            return it->second;
        if (!FileExists(path.c_str()))
            return std::unexpected("Sound not found: " + path);
        auto snd = LoadSound(path.c_str());
        sounds_[name] = snd;
        return snd;
    }

    std::expected<Font, std::string> load_font(const std::string& name, const std::string& path) {
        if (auto it = fonts_.find(name); it != fonts_.end())
            return it->second;
        if (!FileExists(path.c_str()))
            return std::unexpected("Font not found: " + path);
        auto fnt = LoadFont(path.c_str());
        fonts_[name] = fnt;
        return fnt;
    }

    std::expected<Music, std::string> load_music(const std::string& name, const std::string& path) {
        if (auto it = music_.find(name); it != music_.end())
            return it->second;
        if (!FileExists(path.c_str()))
            return std::unexpected("Music not found: " + path);
        auto mus = LoadMusicStream(path.c_str());
        music_[name] = mus;
        return mus;
    }

    Texture2D* get_texture(const std::string& name) {
        auto it = textures_.find(name);
        return it != textures_.end() ? &it->second : nullptr;
    }

    Sound* get_sound(const std::string& name) {
        auto it = sounds_.find(name);
        return it != sounds_.end() ? &it->second : nullptr;
    }

    Font* get_font(const std::string& name) {
        auto it = fonts_.find(name);
        return it != fonts_.end() ? &it->second : nullptr;
    }

    Music* get_music(const std::string& name) {
        auto it = music_.find(name);
        return it != music_.end() ? &it->second : nullptr;
    }

private:
    std::unordered_map<std::string, Texture2D> textures_;
    std::unordered_map<std::string, Sound> sounds_;
    std::unordered_map<std::string, Font> fonts_;
    std::unordered_map<std::string, Music> music_;
};

} // namespace ls
