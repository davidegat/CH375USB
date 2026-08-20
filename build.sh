#!/usr/bin/env bash
# CH375USB build script
# Author: Davide "gat" - https://github.com/davidegat
# Copyright (C) 2026 Davide "gat"
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail
cd "$(dirname "$0")"
nasm -f bin CH375USB.ASM -o CH375USB.SYS
printf 'Built CH375USB.SYS (%s bytes)\n' "$(stat -c%s CH375USB.SYS)"
