#pragma once

#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/tab_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace toolkit {
namespace editor {

// 前置声明
class MinigamePanel;
class TapTapPanel;
class SettingsPanel;

// 主插件停靠面板
class GodotMinigameDock : public godot::MarginContainer {
    GDCLASS(GodotMinigameDock, godot::MarginContainer);

private:
    godot::TabContainer* tab_container = nullptr;

    // 四个功能面板
    MinigamePanel* minigame_panel = nullptr;
    TapTapPanel* taptap_panel = nullptr;
    SettingsPanel* settings_panel = nullptr;

protected:
    static void _bind_methods();

public:
    GodotMinigameDock();
    ~GodotMinigameDock();

    virtual void _ready() override;
    virtual void _exit_tree() override;

    // Panel management
    void create_panels();
    void update_panels();
    void refresh_all_panels();

    // Access to sub-panels
    MinigamePanel* get_minigame_panel() const { return minigame_panel; }
    TapTapPanel* get_taptap_panel() const { return taptap_panel; }
    SettingsPanel* get_settings_panel() const { return settings_panel; }
};

} // namespace editor
} // namespace toolkit
