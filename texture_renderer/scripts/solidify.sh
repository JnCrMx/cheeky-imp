#!/bin/bash

SCRIPT=$(realpath "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

N=16

if [ -z "$1" ]
then
    echo "Usage: $0 WORK-DIR"
    exit 1
fi

dir=$1

for d in $dir/*/; do
    event=$(basename ${d%/})
    echo $event
    if [ ! -z $FORCE ] || [ ! -f "$dir/$event/texture_solid.png" ]; then
        ((i=i%N)); ((i++==0)) && wait
        gmic input "$d/texture.png" +solidify 0,0,0 -output[1] "$d/texture_solid.png" &
    fi
done
wait
