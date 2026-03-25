# Copyright (C) 2026 Lancelot MEI
# SPDX-License-Identifier: GPL-2.0-only

# Suppress "unique_unit_address_if_enabled" to handle some overlaps
list(APPEND EXTRA_DTC_FLAGS "-Wno-unique_unit_address_if_enabled")
