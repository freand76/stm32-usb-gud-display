#!/bin/sh
# Download xPack OpenOCD into toolchain/ and check its sha256.
#
# Usage: tools/fetch_openocd.sh [destination]
set -eu

VERSION=0.12.0-7
NAME=xpack-openocd-${VERSION}
URL=https://github.com/xpack-dev-tools/openocd-xpack/releases/download/v${VERSION}/${NAME}-linux-x64.tar.gz

DEST=${1:-$(dirname "$0")/../toolchain}
mkdir -p "$DEST"
DEST=$(cd "$DEST" && pwd)

if [ -x "$DEST/$NAME/bin/openocd" ]; then
    echo "OpenOCD already in $DEST/$NAME"
    exit 0
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

echo "Downloading $URL"
curl -fL --progress-bar -o "$TMP/$NAME.tar.gz" "$URL"
curl -fsL -o "$TMP/$NAME.tar.gz.sha" "$URL.sha"
(cd "$TMP" && awk '{print $1 "  '"$NAME"'.tar.gz"}' "$NAME.tar.gz.sha" | sha256sum -c -)

tar -xzf "$TMP/$NAME.tar.gz" -C "$DEST"
ln -sfn "$NAME" "$DEST/openocd"
"$DEST/openocd/bin/openocd" --version 2>&1 | head -1
