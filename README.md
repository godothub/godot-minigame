# Toolkit Addons

面向 Godot 4.4+ 的 C++ 编辑器插件，提供小游戏模板适配、下载、缓存、解压和导出能力。

插件本体更新不通过模板分发流程管理。模板通过 Release 资产分发，插件本身建议通过 Godot Asset Library 或仓库源码安装更新。

## 功能

- 按当前 Godot 版本自动匹配模板
- 支持 GitHub / Gitee Release 作为模板分发源
- 首次下载模板，后续走本地缓存
- 导出时自动解压模板并生成小游戏目录
- 支持将资源嵌入扩展产物

## 构建

```bash
git clone <repo-url>
git submodule update --init --recursive
./build_osx.sh
```

也可以按平台选择：

- `./build_win.sh`
- `./build_linux.sh`
- `./build_osx.sh`

构建产物默认会安装到：

`demo/addons/toolkit-addons/bin/<platform>/`

## 使用

1. 将 `demo/addons/toolkit-addons/` 放到你的 Godot 项目 `res://addons/toolkit-addons/`
2. 在编辑器里启用插件
3. 在设置面板配置模板分发源：
   `Source / Owner / Repo / Tag`
4. 导出时插件会自动选择模板并完成下载、缓存和解压

## 模板分发约定

Release 需要提供：

- `versions.yaml`
- `versions.yaml` 中引用的 `*.tpz`

示例：

```yaml
godot4:
  4.5.1: minigame4.5.1.1.tpz
```

当前匹配规则：

- 先找当前引擎版本的精确匹配
- 找不到则选择同大版本下不高于目标版本的最新模板
- 还找不到则回退到该大版本桶里的最后一个条目

## 目录

- `src/`：源码
- `include/`：头文件
- `resources/`：嵌入资源
- `demo/`：示例项目
- `tools/`：构建辅助脚本
- `godot-cpp/`：submodule

## License

MIT，见 `LICENSE`。
