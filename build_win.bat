@echo off
setlocal

scons platform=windows arch=x86_64 target=template_release embed_resources=yes || exit /b 1
scons platform=windows arch=x86_32 target=template_release embed_resources=yes || exit /b 1
