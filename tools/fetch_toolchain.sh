#!/bin/sh
# Download the Arm GNU Toolchain (arm-none-eabi) into toolchain/ and check its sha256.
#
# Usage: tools/fetch_toolchain.sh [destination]
set -eu

VERSION=15.2.rel1
HOST=x86_64
NAME=arm-gnu-toolchain-${VERSION}-${HOST}-arm-none-eabi
URL=https://developer.arm.com/-/media/Files/downloads/gnu/${VERSION}/binrel/${NAME}.tar.xz

DEST=${1:-$(dirname "$0")/../toolchain}
mkdir -p "$DEST"
DEST=$(cd "$DEST" && pwd)

if [ -x "$DEST/$NAME/bin/arm-none-eabi-gcc" ]; then
    echo "Toolchain already in $DEST/$NAME"
    exit 0
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

echo "Downloading $URL"
curl -fL --progress-bar -o "$TMP/$NAME.tar.xz" "$URL"
curl -fsL -o "$TMP/$NAME.tar.xz.sha256asc" "$URL.sha256asc"
(cd "$TMP" && awk '{print $1 "  '"$NAME"'.tar.xz"}' "$NAME.tar.xz.sha256asc" | sha256sum -c -)

tar -xJf "$TMP/$NAME.tar.xz" -C "$DEST"
ln -sfn "$NAME" "$DEST/current"
"$DEST/current/bin/arm-none-eabi-gcc" --version | head -1
