#!/bin/sh
set -eu
umask 022

usage()
{
    echo "Usage: $0 [LXQT_PANEL_SOURCE_DIR] [PACKAGE_VERSION]" >&2
    exit 2
}

[ "$#" -le 2 ] || usage

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
panel_source=${1:-"$project_dir/build/lxqt-panel-2.3.2"}
package_version=${2:-1.0.8-1}
build_dir="$project_dir/build/lxqt-panel-2.3.2-resource-monitor"
plugin_build_dir="$build_dir/plugin-resourcemonitor"
dpkg --validate-version "$package_version"

if ! git -C "$panel_source" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    if [ -e "$panel_source" ]; then
        echo "Panel source directory exists but is not a Git checkout:" >&2
        echo "  $panel_source" >&2
        exit 1
    fi
    mkdir -p "$(dirname -- "$panel_source")"
    git clone --depth 1 --branch 2.3.2 \
        https://github.com/lxqt/lxqt-panel.git "$panel_source"
fi

mkdir -p "$panel_source/plugin-resourcemonitor"
# The package build uses this repository's CMake project and copies its plugin
# sources next to the upstream panel headers. It does not configure other
# panel components or plugins.
cp -a "$project_dir/plugin-resourcemonitor/." "$panel_source/plugin-resourcemonitor/"

cmake -S "$project_dir/packaging/standalone" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLXQT_PANEL_SOURCE_DIR="$panel_source"
cmake --build "$build_dir" --target resourcemonitor resourcemonitor-netcap \
    --parallel "${BUILD_JOBS:-2}"
"$project_dir/packaging/build-deb.sh" \
    "$plugin_build_dir" "$package_version" "$project_dir/build/packages"
