# This is how I build DDNet for Android:

# Cloning the building repo with the SDL port for Android by Pelya
cd /media
git clone https://github.com/pelya/commandergenius.git

# Get the most recent DDNet source
cd /media/commandergenius/project/jni/application/teeworlds
rm -rf src master.zip*
wget "https://github.com/ddnet/ddnet/archive/master.zip"
unzip ddnet-master.zip
mv ddnet-master src
mkdir src/src/game/generated
# Also the generated files don't get generated, copy them by hand
cp /media/ddrace/src/game/generated/* src/src/game/generated
rm -rf AndroidData
./AndroidPreBuild.sh

# Actual compilation, needs a key to sign
cd /media/commandergenius
ln -s teeworlds project/jni/application/src
./changeAppSettings.sh -a
android update project -p project
./build.sh
jarsigner -verbose -keystore ~/.android/release.keystore -storepass MYSECRETPASS -sigalg MD5withRSA -digestalg SHA1 project/bin/MainActivity-release-unsigned.apk androidreleasekey
zipalign 4 project/bin/MainActivity-release-unsigned.apk project/bin/MainActivity-release.apk
scp project/bin/MainActivity-release.apk ddnet:/var/www/downloads/DDNet-$VERSION.apk
