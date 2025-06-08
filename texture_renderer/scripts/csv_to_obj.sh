#!/bin/bash

SCRIPT="$(realpath "$0")"
SCRIPTPATH="$(dirname "$SCRIPT")"

csv_dir="$1"
obj_dir="$2"

if [ -z "$csv_dir" ] || [ -z "$obj_dir" ]
then
    echo "Usage: $0 CSV-DIR OBJ-DIR"
    exit 1
fi

N=12
script="$(realpath csv_to_obj.py)"

mkdir -p "$obj_dir"

for f in "$csv_dir"/*.csv
do
    echo "Processing $f"
    ((i=i%N)); ((i++==0)) && wait
    python3 "$SCRIPTPATH/lib/csv_to_obj.py" "$f" > "$obj_dir/$(basename $f .csv).obj" &
done
wait
