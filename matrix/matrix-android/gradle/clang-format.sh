#!/usr/bin/env bash

find ../ \( -name "*.cpp" -or -name "*.hpp" -or -name "*.h" -or -name "*.cc" \) | xargs clang-format -i