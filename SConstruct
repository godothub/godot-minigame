#!/usr/bin/env python
import os
import sys

from methods import print_error

libname = "toolkit-addons"
projectdir = "demo"

localEnv = Environment(tools=["default"], PLATFORM="")

# 读取命令行的 platform / arch（如果 custom.py 没有定义，可以用 ARGUMENTS 读取）
platform = ARGUMENTS.get("platform", "windows")  # 默认 windows，自行调整
arch = ARGUMENTS.get("arch", "x86_64")

localEnv["platform"] = platform
localEnv["arch"] = arch

# 如果要用 osxcross 交叉编译 macOS
if platform == "macos":
    # 建议在外部 shell 中已 export OSXCROSS_ROOT 和 PATH
    osxcross_root = os.environ.get("OSXCROSS_ROOT", "/opt/osxcross")  # 按需修改
    
    # 这里的 triplet 要和你 build osxcross 时生成的一致，可以在 $OSXCROSS_ROOT/target/bin 里 ls 看看
    triplet = "x86_64-apple-darwin25.1"  # 示例：macOS 14 SDK，对 x86_64
    if arch == "arm64":
        triplet = "aarch64-apple-darwin25.1"
    
    localEnv["CC"] = f"{triplet}-clang"
    localEnv["CXX"] = f"{triplet}-clang++"
    
    # macOS 动态库后缀
    localEnv["SHLIBSUFFIX"] = ".dylib"
    
    # 最小部署版本、C++ 标准库等
    min_ver = os.environ.get("MACOSX_DEPLOYMENT_TARGET", "14.0")
    localEnv.Append(CCFLAGS=[
        "-std=c++17",
        f"-mmacosx-version-min={min_ver}",
        "-stdlib=libc++",
    ])
    localEnv.Append(LINKFLAGS=[
        f"-mmacosx-version-min={min_ver}",
        "-stdlib=libc++",
    ])


# Build profiles can be used to decrease compile times.
# You can either specify "disabled_classes", OR
# explicitly specify "enabled_classes" which disables all other classes.
# Modify the example file as needed and uncomment the line below or
# manually specify the build_profile parameter when running SCons.

# localEnv["build_profile"] = "build_profile.json"

customs = ["custom.py"]
customs = [os.path.abspath(path) for path in customs]

opts = Variables(customs, ARGUMENTS)
opts.Update(localEnv)

Help(opts.GenerateHelpText(localEnv))

env = localEnv.Clone()

if not (os.path.isdir("godot-cpp") and os.listdir("godot-cpp")):
    print_error("""godot-cpp is not available within this folder, as Git submodules haven't been initialized.
Run the following command to download godot-cpp:

    git submodule update --init --recursive""")
    sys.exit(1)

env = SConscript("godot-cpp/SConstruct", {"env": env, "customs": customs})

env.Append(CPPPATH=["src/", "include/"])

# Add all subdirectories recursively
import os
sources = []
for root, dirs, files in os.walk("src"):
    for file in files:
        if file.endswith(".cpp") and not file.endswith(".gen.cpp"):
            sources.append(os.path.join(root, file).replace("\\", "/"))

if env["target"] in ["editor", "template_debug"]:
    try:
        doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
        sources.append(doc_data)
    except AttributeError:
        print("Not including class reference as we're targeting a pre-4.3 baseline.")

# Resource embedding support
if ARGUMENTS.get("embed_resources", "no") == "yes":
    print("Building with embedded resources...")
    env.Append(CPPDEFINES=["EMBED_RESOURCES"])
    
    # Add resource files to build
    resource_files = []
    if os.path.exists("resources"):
        for root, dirs, files in os.walk("resources"):
            for file in files:
                if file.endswith((".zip", ".tpz", ".js", ".json", ".yaml", ".yml", ".svg", ".png", ".jpg", ".jpeg", ".webp")):
                    resource_files.append(os.path.join(root, file))
    
    if resource_files:
        # Generate resource header
        python_exec = sys.executable
        resource_gen = env.Command("src/resources/embedded_resources.gen.cpp", 
                                 resource_files,
                                 f'"{python_exec}" tools/embed_resources.py $SOURCES $TARGET')
        sources.append(resource_gen)

# .dev doesn't inhibit compatibility, so we don't need to key it.
# .universal just means "compatible with all relevant arches" so we don't need to key it.
suffix = env['suffix'].replace(".dev", "").replace(".universal", "").replace(".template_debug", "").replace(".template_release", "")

lib_filename = "{}{}{}{}".format(env.subst('$SHLIBPREFIX'), libname, suffix, env.subst('$SHLIBSUFFIX'))

library = env.SharedLibrary(
    "bin/{}/{}".format(env['platform'], lib_filename),
    source=sources,
)

copy = env.Install("{}/addons/toolkit-addons/bin/{}/".format(projectdir, env["platform"]), library)

default_args = [library, copy]
Default(*default_args)
