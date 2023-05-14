#!/bin/sh

BASE_PATH=$2

dir=$1
out=${3-"./out"}
mkdir -p $out

for d in $dir/*/; do
    event=$(basename ${d%/})
    echo $event
    cp "$dir/$event/texture.png" "$out/$event.png"
    cat > "$out/$event.mtl" <<EOF
newmtl $event
Ns 250.000000
Ka 1.000000 1.000000 1.000000
Ks 0.500000 0.500000 0.500000
Ke 0.000000 0.000000 0.000000
Ni 1.450000
d 1.000000
illum 2
map_Kd $event.png
EOF
    for f in "$BASE_PATH"/out/$event-*.obj; do
        filename="$(basename "$f")"
        objectname="${filename%.obj}"
        cat > "$out/$filename" <<EOF
mtllib $event.mtl
usemtl $event
o $objectname
EOF
        grep -E '^v ' --color=never < "$f" >> "$out/$filename"
        grep -E '^(vt|f )' --color=never < "$dir/$event/model-uv.obj" >> "$out/$filename"
    done
done
