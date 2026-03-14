@tool
extends EditorPlugin

var dock
var wechat_platform
var wechat_platform_added := false

func _enter_tree():
	print("[Toolkit Addons][plugin.gd] _enter_tree")
	_initialize_plugin()

func _exit_tree():
	print("[Toolkit Addons][plugin.gd] _exit_tree")
	_unregister_wechat_platform()

	# The editor is responsible for cleaning up the dock control.
	# Manually removing it in _exit_tree can cause errors during the editor's
	# shutdown sequence, as the dock manager might already be dismantled.
	if is_instance_valid(dock):
		dock.queue_free()

func _initialize_plugin():
	dock = ToolkitDock.new()
	add_control_to_bottom_panel(dock, "工具箱")

	_register_wechat_platform()

func _register_wechat_platform():
	print("[Toolkit Addons][plugin.gd] _register_wechat_platform begin")
	if wechat_platform_added and wechat_platform and is_instance_valid(wechat_platform):
		print("[Toolkit Addons][plugin.gd] platform already added, skip")
		return

	if not ClassDB.class_exists("WeChatExportPlatform"):
		push_warning("Toolkit Addons: WeChatExportPlatform class is not available")
		printerr("[Toolkit Addons][plugin.gd] ClassDB missing WeChatExportPlatform")
		return

	if not wechat_platform or not is_instance_valid(wechat_platform):
		wechat_platform = WeChatExportPlatform.new()
		print("[Toolkit Addons][plugin.gd] new WeChatExportPlatform -> ", wechat_platform)

	add_export_platform(wechat_platform)
	wechat_platform_added = true
	print("[Toolkit Addons][plugin.gd] add_export_platform success, added=", wechat_platform_added)

func _unregister_wechat_platform():
	print("[Toolkit Addons][plugin.gd] _unregister_wechat_platform begin, added=", wechat_platform_added, " valid=", is_instance_valid(wechat_platform))
	if wechat_platform_added and wechat_platform and is_instance_valid(wechat_platform):
		remove_export_platform(wechat_platform)
		print("[Toolkit Addons][plugin.gd] remove_export_platform success")

	wechat_platform_added = false
	wechat_platform = null
