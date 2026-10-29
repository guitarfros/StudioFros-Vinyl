#!/usr/bin/env bash
# Builds the project (Linux / macOS) and shows where the results ended up.
set -e
cd "$(dirname "$0")"
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j"$JOBS"

echo
echo "== Build results =="
find build/*_artefacts -maxdepth 3 -iname "*vinyl*"
echo
case "$(uname -s)" in
  Darwin) echo "== Installed copy =="; ls -la ~/Library/Audio/Plug-Ins/VST3 | grep -i vinyl || echo "(no copy found)";;
  *)      echo "== Installed copy =="; ls -la ~/.vst3 2>/dev/null | grep -i vinyl || echo "(no copy found in ~/.vst3)";;
esac
