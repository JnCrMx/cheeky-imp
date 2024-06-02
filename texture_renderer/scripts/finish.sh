#!/bin/sh

workdir=$1
eventdir=$2
out=${3-"./final"}
mkdir -p $out

if [ -z "$workdir" ] || [ -z "$eventdir" ] || [ -z "$out" ]
then
    echo "Usage: $0 WORK-DIR OBJ-DIR FINAL-DIR"
    exit 1
fi

for d in $workdir/*/; do
    event=$(basename ${d%/})
    echo $event
    if [ -f "$workdir/$event/texture_solid.png" ]; then
        cp "$workdir/$event/texture_solid.png" "$out/$event.png"
    else
        cp "$workdir/$event/texture.png" "$out/$event.png"
    fi
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
    for f in "$eventdir"/$event-*.obj; do
        if ! [ -f "$f" ]; then continue; fi
        filename="$(basename "$f")"
        objectname="${filename%.obj}"
        cat > "$out/$filename" <<EOF
mtllib $event.mtl
usemtl $event
o $objectname
EOF
        grep -E '^v ' --color=never < "$f" >> "$out/$filename"
        grep -E '^(vt|f )' --color=never < "$workdir/$event/model-uv.obj" >> "$out/$filename"
    done
done
