#!/usr/bin/env zsh

set -e -x
setopt extended_glob

function mcp {
  cp "$1" "$2.$$.tmp" && mv "$2.$$.tmp" "$2"
}

OLD_VERSION=$1
VERSION=$2

renice -n 19 -p $$
ionice -c 3 -p $$

# Set directories
SCRIPTS_DIR="/var/www/tclient.app/scripts"
UPDATES_DIR="/var/www/tclient.app/updates"

cd "$SCRIPTS_DIR"

# Download from github
wget -O TClient-${OLD_VERSION}-win64.zip "https://github.com/sjrc6/TaterClient-ddnet/releases/download/V${OLD_VERSION}/TClient-windows.zip"
wget -O TClient-${OLD_VERSION}-linux_x86_64.tar.xz "https://github.com/sjrc6/TaterClient-ddnet/releases/download/V${OLD_VERSION}/TClient-ubuntu.tar.xz"

wget -O TClient-${VERSION}-win64.zip "https://github.com/sjrc6/TaterClient-ddnet/releases/download/V${VERSION}/TClient-windows.zip"
wget -O TClient-${VERSION}-linux_x86_64.tar.xz "https://github.com/sjrc6/TaterClient-ddnet/releases/download/V${VERSION}/TClient-ubuntu.tar.xz"

# Unpack directories
unzip -d TClient-${OLD_VERSION}-win64 TClient-${OLD_VERSION}-win64.zip
unzip -d TClient-${VERSION}-win64 TClient-${VERSION}-win64.zip

for ver in $OLD_VERSION $VERSION; do
  WIN_DIR="TClient-${ver}-win64"
  inner_dirs=("${(@f)$(find "$WIN_DIR" -mindepth 1 -maxdepth 1 -type d)}")
  if [ ${#inner_dirs} -eq 1 ]; then
    mv "${inner_dirs[1]}"/* "$WIN_DIR/"
    rmdir "${inner_dirs[1]}"
  fi
done

mkdir -p TClient-${OLD_VERSION}-linux_x86_64
tar --strip-components=1 -xvf TClient-${OLD_VERSION}-linux_x86_64.tar.xz -C TClient-${OLD_VERSION}-linux_x86_64

mkdir -p TClient-${VERSION}-linux_x86_64
tar --strip-components=1 -xvf TClient-${VERSION}-linux_x86_64.tar.xz -C TClient-${VERSION}-linux_x86_64

# fetch update.json from the served files
if [ -f "${UPDATES_DIR}/update.json" ]; then
  cp "${UPDATES_DIR}/update.json" .
else
  echo "[]" > update.json
fi

# diff versions
./diff_update.py $OLD_VERSION $VERSION

cp update.json update.json.old && mv update.json.new update.json

if [ -d "${UPDATES_DIR}/data" ]; then
  mv "${UPDATES_DIR}/data" "${UPDATES_DIR}/data.old"
fi
mv TClient-${VERSION}-win64/data "${UPDATES_DIR}/data"
rm -rf "${UPDATES_DIR}/data.old"

mv TClient-${VERSION}-win64/license.txt ${UPDATES_DIR}/
mv TClient-${VERSION}-win64/storage.cfg ${UPDATES_DIR}/
mv TClient-${VERSION}-win64/config_directory.bat ${UPDATES_DIR}/

for i in TClient-${VERSION}-win64/*.{exe,dll}; do 
  mcp "$i" "${i:r:t}-win64.${i:e}"
  mv "${i:r:t}-win64.${i:e}" "${UPDATES_DIR}/"
done

for i in TClient-${VERSION}-linux_x86_64/{DDNet,DDNet-Server}; do 
  mcp "$i" "${i:r:t}-linux-x86_64"
  mv "${i:r:t}-linux-x86_64" "${UPDATES_DIR}/"
done

if ls TClient-${VERSION}-linux_x86_64/*.so 2>&1 > /dev/null; then
  for i in TClient-${VERSION}-linux_x86_64/*.so; do 
    mcp "$i" "${i:r:t}-linux-x86_64.so"
    mv "${i:r:t}-linux-x86_64.so" "${UPDATES_DIR}/"
  done
fi

mv update.json ${UPDATES_DIR}/

rm -rf TClient-${OLD_VERSION}-win64 TClient-${OLD_VERSION}-linux_x86_64
rm -rf TClient-${VERSION}-win64 TClient-${VERSION}-linux_x86_64
rm TClient-${OLD_VERSION}-win64.zip TClient-${OLD_VERSION}-linux_x86_64.tar.xz
rm TClient-${VERSION}-win64.zip TClient-${VERSION}-linux_x86_64.tar.xz

cat <<EOF > ${UPDATES_DIR}/info.json
{
  "version": "${VERSION}"
}
EOF
