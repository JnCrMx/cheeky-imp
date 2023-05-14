#!/bin/bash

SCRIPT=$(realpath "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

BASE_PATH=$2

dir=$1

for d in $dir/*/; do
    event=$(basename ${d%/})
    echo $event
    cp "$BASE_PATH"/in/$event-0.csv $dir/$event/vertices.csv
    cp "$BASE_PATH"/out/$event-0.obj $dir/$event/model.obj
    spirv-cross -V --version 450 $dir/$event/shader.frag.spv > $dir/$event/shader.frag
    if [ ! -f "$dir/$event/model-uv.obj" ]; then
        blender --background --python $SCRIPTPATH/generate_uv.py -- "$dir/$event/model.obj" "$dir/$event/model-uv.obj" #>/dev/null 2>/dev/null
    fi
done
