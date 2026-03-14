#!/bin/bash
set -e
scons platform=windows arch=x86_64 target=template_release
scons platform=windows arch=x86_32 target=template_release
