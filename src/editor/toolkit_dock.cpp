#include "editor/toolkit_dock.h"
#include "editor/minigame_panel.h"
#include "editor/taptap_panel.h"
#include "editor/settings_panel.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/style_box_empty.hpp>
#include <godot_cpp/classes/engine.hpp>
#include "core/logging.h"

using namespace godot;

namespace toolkit
{
    namespace editor
    {

        ToolkitDock::ToolkitDock()
        {
            set_name(String::utf8("工具箱"));
            // 设置边距，完全参考Godot调试面板的样式
            // 使用更合适的边距值，适合编辑器面板
            add_theme_constant_override("margin_left", -4);
            add_theme_constant_override("margin_right", -4);
            add_theme_constant_override("margin_top", -4);
        }

        ToolkitDock::~ToolkitDock()
        {
            // Cleanup handled by Godot
        }

        void ToolkitDock::_bind_methods()
        {
            ClassDB::bind_method(D_METHOD("create_panels"), &ToolkitDock::create_panels);
            ClassDB::bind_method(D_METHOD("update_panels"), &ToolkitDock::update_panels);
            ClassDB::bind_method(D_METHOD("refresh_all_panels"), &ToolkitDock::refresh_all_panels);
        }

        void ToolkitDock::_ready()
        {
            TOOLKIT_LOG("ToolkitDock: Ready");
            
            // Ensure template system is initialized even if MinigamePanel is hidden
            if (Engine::get_singleton()->has_singleton("GodotToolkit")) {
                Object* toolkit_core = Engine::get_singleton()->get_singleton("GodotToolkit");
                if (toolkit_core && toolkit_core->has_method("initialize_template_system")) {
                    toolkit_core->call("initialize_template_system");
                }
            }

            create_panels();
        }

        void ToolkitDock::_exit_tree()
        {
            TOOLKIT_LOG("ToolkitDock: Exit tree");
        }

        void ToolkitDock::create_panels()
        {
            // 直接创建tab container，MarginContainer作为根节点提供边距
            tab_container = memnew(TabContainer);
            tab_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            tab_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
            tab_container->set_custom_minimum_size(Vector2(300, 200)); // 确保有足够空间显示所有标签

            Ref<StyleBoxEmpty> empty;
            empty.instantiate();
            tab_container->add_theme_stylebox_override("panel", empty);

            // 设置标签栏样式，类似编辑器面板
            tab_container->set_tabs_rearrange_group(-1); // 禁用拖拽重排序
            add_child(tab_container);

            /*
            // Create minigame panel (小游戏)
            minigame_panel = memnew(MinigamePanel);
            minigame_panel->set_name(String::utf8("小游戏"));
            tab_container->add_child(minigame_panel);

            // Create TapTap panel
            taptap_panel = memnew(TapTapPanel);
            taptap_panel->set_name("TapTap");
            tab_container->add_child(taptap_panel);
            */

            // Create settings panel (设置)
            settings_panel = memnew(SettingsPanel);
            settings_panel->set_name(String::utf8("设置"));
            tab_container->add_child(settings_panel);

            TOOLKIT_LOG("ToolkitDock: All panels created successfully");
        }

        void ToolkitDock::update_panels()
        {
            // Update all panels with current data
            if (minigame_panel)
            {
                // Update minigame panel if needed
                minigame_panel->update_button_state();
            }

            if (taptap_panel)
            {
                // Update TapTap panel if needed
            }

            if (settings_panel)
            {
                // Update settings panel if needed
            }
        }

        void ToolkitDock::refresh_all_panels()
        {
            update_panels();
            TOOLKIT_LOG("ToolkitDock: All panels refreshed");
        }

    } // namespace editor
} // namespace toolkit
