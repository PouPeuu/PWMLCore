#!/bin/bash
set -e

SOURCE_DIRECTORY="src"
INCLUDE_DIRECTORY="include"

LIB_INSTALL_DIRECTORY="/usr/local/lib"
INCLUDE_INSTALL_DIRECTORY="/usr/local/include"
NAME="PWML"
PKG_CONFIG_DEPENDENCIES="glib-2.0 json-c"
EXTRA_FLAGS="-g $(xml2-config --cflags --libs) -O0 -Wall -Wextra -pedantic -Werror"

RETURN_WORKING_DIRECTORY=$(pwd)
cd /home/poupeuu/Coding/C/PWML/PWMLCore

if ! test -d build 
then
    echo Creating Build directory
    mkdir build
fi

echo Compiling object files
for src in "$SOURCE_DIRECTORY"/*.c; do
    obj_name="$(basename "$src" .c).o"
    gcc -I"$INCLUDE_DIRECTORY" -c "$src" -o "build/$obj_name" $(pkg-config --cflags --libs $PKG_CONFIG_DEPENDENCIES) $EXTRA_FLAGS
done

echo Creating static library
ar rcs "build/lib$NAME.a" build/*.o

if [ -w "$LIB_INSTALL_DIRECTORY" ] && [ -w "$INCLUDE_INSTALL_DIRECTORY" ]; then
    echo Installing
    cp "build/lib$NAME.a" "$LIB_INSTALL_DIRECTORY/"
    cp -r "$INCLUDE_DIRECTORY"/* "$INCLUDE_INSTALL_DIRECTORY/"
else
    echo Not installed due to insufficient permissions
fi

cd "$RETURN_WORKING_DIRECTORY"
