# Copyright (C) 2026 Lancelot MEI
# SPDX-License-Identifier: GPL-2.0-only

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(BLEtracker)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
