#!/bin/bash

SCRIPT=$(realpath "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

N=16

attachment=1
samples=4
width=2048
height=2048
uvscale=1.0

if [ -z "$1" ]
then
    echo "Usage: $0 WORK-DIR"
    exit 1
fi

dir=$1

for d in $dir/*/; do
    event=$(basename ${d%/})
    echo $event
    if [ ! -z $FORCE ] || [ ! -f "$dir/$event/texture.png" ]; then
        ((i=i%N)); ((i++==0)) && wait
        $SCRIPTPATH/../build/texture_renderer --output $dir/$event/texture.png \
            --attachment $attachment --samples $samples --width $width --height $height \
            --shader $dir/$event/shader.frag --vertices $dir/$event/vertices.csv \
            --pipeline $dir/$event/pipeline.json --obj $dir/$event/model-uv.obj \
            --uv-scale $uvscale &
    fi
done
wait
