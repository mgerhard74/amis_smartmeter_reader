#! /bin/zsh
## vim:set ft=zsh ts=4 sw=4 et:
set -e; unsetopt NULL_GLOB; LC_COLLATE=C


self=$0
selfdir=${self:h}

(
cd ${selfdir}
./upload_files.zsh ../../.pio/build/esp12e/firmware.bin
)
