#!/bin/bash

apt update -y && apt upgrade -y
apt install build-essential unzip awscli -y

git clone https://github.com/jhonnold/berserk && \
cd berserk/src && \
git checkout nnue-fen-gen-alt && \
make basic EXE=generator && \
mv generator ../.. && \
cd ../..

wget https://github.com/official-stockfish/books/raw/master/noob_3moves.epd.zip && \
unzip noob_3moves.epd.zip

for i in {0..999}
do
    ./generator --threads=128 --total=10000000 --book=noob_3moves.epd

    zstd -T16 -o "berserk.mpv.min5k.$i.zstd" berserk*.fens
    aws s3 cp "berserk.mpv.min5k.$i.zstd" s3://berserk-mpv-data

    rm berserk*.fens
    rm "berserk.mpv.min5k.$i.zstd"
done
