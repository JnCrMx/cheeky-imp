#!/use/bin/env fish

set VH_ROOT $(realpath $PWD)
while [ ! -d "$VH_ROOT/.stuff" ]
	set VH_ROOT $(realpath $VH_ROOT/..)
end

echo "Root directory is $VH_ROOT"

set -x VH_ROOT "$VH_ROOT"

set -x VH_BIN "$VH_ROOT/.stuff/bin"
set -x VH_LIB "$VH_ROOT/.stuff/lib"
set -x PATH "$PATH":"$VH_BIN"

set -x VH_LAYER_ROOT "$VH_ROOT/layer"
set -x VH_LAYER_DUMP "$VH_LAYER_ROOT/dump"
set -x VH_LAYER_OVERRIDE "$VH_LAYER_ROOT/override"

set -x VH_WORKSPACE "$VH_ROOT/workspace"

if [ -f "$VH_WORKSPACE/config.fish" ]
	source $VH_WORKSPACE/config.fish
end
