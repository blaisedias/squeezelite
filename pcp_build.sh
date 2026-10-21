#!/bin/sh
tce-load -w pcp-libffmpeg-dev pcp-libogg-dev pcp-libflac-dev pcp-libvorbis-dev pcp-libmad-dev pcp-libfaad2-dev pcp-libsoxr-dev pcp-libmpg123-dev pcp-lirc-dev openssl-dev pcp-libopus-dev libgpiod-dev
tce-load -il pcp-libffmpeg-dev pcp-libogg-dev pcp-libflac-dev pcp-libvorbis-dev pcp-libmad-dev pcp-libfaad2-dev pcp-libsoxr-dev pcp-libmpg123-dev pcp-lirc-dev openssl-dev pcp-libopus-dev libgpiod-dev
tce-load -il git compiletc

make clean

BUILDOPTIONS="-DRESAMPLE -DFFMPEG -DOPUS -DVISEXPORT -DGPIO -DRPI -DIR -DUSE_SSL -DCUSTOM_VERSION=-pCP"

#make OPTS="$BUILDOPTIONS"
make CFLAGS="-I/usr/local/include/opus $CPUOPTS" LDFLAGS="-s" OPTS="$BUILDOPTIONS" 
