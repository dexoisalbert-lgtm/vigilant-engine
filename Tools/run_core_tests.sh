#!/usr/bin/env bash
# Builds and runs the engine-free core tests. No Unreal installation needed.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CORE="$ROOT/Source/PaleotrenchCore"

# Guard: engine-free code must not reach for Unreal.
if grep -rnE '#include "(CoreMinimal|Modules/|UObject/|Engine/|Containers/|Math/|HAL/|Misc/)' \
	"$CORE/Public" "$CORE/Private" --include='*.h' --include='*.cpp' \
	| grep -v "/Private/Module/"; then
	echo "ERROR: Unreal header included from engine-free core code (see above)." >&2
	exit 1
fi

BUILD="$ROOT/Tools/CoreTests/build"
cmake -S "$ROOT/Tools/CoreTests" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$BUILD" -j > /dev/null
"$BUILD/core_tests"
