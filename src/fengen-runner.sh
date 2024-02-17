#!/bin/bash

set -uex

thread_count=$(($(nproc --all) - 1));
host_name=$(hostname | cut -c13-)
version=$(grep -Po '(?<=VERSION  = )[\d]+' makefile)
network=$(grep -Po '(?<=MAIN_NETWORK = berserk-)[A-Za-z0-9]+(?=\.nn)' makefile)
file_name="berserk$version.$network.$host_name"

while true
do
	./berserk --threads $thread_count \
	          --total 1000000 \
	          --output $1 \
	          --nodes 10000 \
	          --depth 0 \
	          --random-move-count 10 \
	          --random-move-min 1 \
	          --random-move-max 10 \
	          --random-multipv 0 \
	          --write-min 16 \
	          --file_name $file_name

	sleep 5
done