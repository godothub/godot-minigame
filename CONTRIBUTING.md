# 贡献指南

感谢你愿意为 Toolkit Addons 做贡献。

## 提交 Issue

- 一个问题一个 Issue（不要把多个不相关问题堆在同一个 Issue 里）
- 请附带：
  - Godot 版本（例如 4.4.0 / 4.5.1）
  - 操作系统与架构（Windows/macOS/Linux，x86_64/arm64）
  - 复现步骤、日志、截图（如果是下载/Release 相关问题，请附上 `owner/repo/tag` 与 `versions.yaml`）

## 开发环境

- Godot 4.4+
- Python 3 + SCons
- 支持 C++17 的编译器
- `godot-cpp` 子模块需要初始化：

```bash
git submodule update --init --recursive
```

## 构建

推荐使用脚本：

```bash
./build_win.sh
./build_linux.sh
./build_osx.sh
```

或直接使用 SCons（示例）：

```bash
scons platform=windows arch=x86_64 target=template_release
scons platform=linux arch=x86_64 target=template_release
scons platform=macos arch=universal target=template_release embed_resources=yes
```

构建产物会安装到：

- `demo/addons/toolkit-addons/bin/<platform>/`

## 模板分发与测试

模板通过 Release 分发，详见：

- `docs/release_distribution.md`
- `docs/release_testing.md`

## 提交 PR

- 尽量小步提交：一个 PR 做一件事
- 不要提交构建产物、Release 产物、以及个人发布脚本（`release/staging/` 与 `.private_release/` 已被忽略）
- PR 描述中请说明：
  - 修改动机
  - 行为变化
  - 如何验证（构建命令/截图/日志）

