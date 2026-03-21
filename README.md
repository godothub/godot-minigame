# Godot Minigame

面向 Godot 4.4+ 的 C++ 编辑器插件，提供小游戏模板适配、下载、缓存、解压和导出能力。

插件本体更新不通过模板分发流程管理。模板通过 Release 资产分发，插件本身建议通过 Godot Asset Library 或仓库源码安装更新。

如果要用 AI 协助做适配移植，请直接使用仓库里的 `skills/` 目录。

## 功能

- 按当前 Godot 版本自动匹配模板
- 支持 GitHub / Gitee Release 作为模板分发源
- 首次下载模板，后续走本地缓存
- 导出时自动解压模板并生成小游戏目录
- 支持将资源嵌入扩展产物
- 提供 `skills/` 作为 AI 适配移植技能目录

## 构建

```bash
git clone <repo-url>
git submodule update --init --recursive
./build_osx.sh
```

也可以按平台选择：

- `build_win.bat`
- `./build_linux.sh`
- `./build_osx.sh`

这些脚本默认都会以 `embed_resources=yes` 构建 release 产物。

构建产物默认会安装到：

`demo/addons/godot-minigame/bin/<platform>/`

## 使用

1. 将 `demo/addons/godot-minigame/` 放到你的 Godot 项目 `res://addons/godot-minigame/`
2. 在编辑器里启用插件
3. 在设置面板配置模板分发源：
   `Source / Owner / Repo / Tag`
4. 导出时插件会自动选择模板并完成下载、缓存和解压

## 模板分发约定

Release 需要提供：

- 索引 Release：全量 `versions.yaml`
- 模板 Release：对应版本的 `*.tpz`

示例：

```yaml
godot4:
  4.5.1:
    tag: 4.5.1
    file: minigame4.5.1.tpz
```

当前匹配规则：

- 先找当前引擎版本的精确匹配
- 找不到则选择同大版本下不高于目标版本的最新模板
- 还找不到则回退到该大版本桶里的最后一个条目

## 目录

- `src/`：源码
- `include/`：头文件
- `resources/`：嵌入资源
- `skills/`：AI 适配移植技能目录
- `demo/`：示例项目
- `tools/`：构建辅助脚本
- `godot-cpp/`：submodule

## License

MIT，见 `LICENSE`。
