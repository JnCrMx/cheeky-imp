#!/bin/bash

SCRIPT=$(realpath "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

workdir=$1
event_csv_dir=$2
event_obj_dir=$3

if [ -z "$workdir" ] || [ -z "$event_csv_dir" ] || [ -z "$event_obj_dir" ]
then
    echo "Usage: $0 WORK-DIR CSV-DIR OBJ-DIR"
    exit 1
fi

for d in $workdir/*/; do
    event=$(basename ${d%/})
    echo $event
    cp "$event_csv_dir/$event-0.csv" "$workdir/$event/vertices.csv"
    cp "$event_obj_dir/$event-0.obj" "$workdir/$event/model.obj"
    spirv-cross -V --version 450 $workdir/$event/shader.frag.spv > $workdir/$event/shader.frag
    if [ ! -f "$workdir/$event/model-uv.obj" ]; then
        blender --background --python $SCRIPTPATH/lib/generate_uvs.py -- "$workdir/$event/model.obj" "$workdir/$event/model-uv.obj" #>/dev/null 2>/dev/null
    fi
done
