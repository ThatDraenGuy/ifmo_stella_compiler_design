#!/bin/sh
BUILD_DIR=userbuild
CMAKE=cmake
LOG_FILE=userbuild.log

$CMAKE -DUSER_SRC_ST=$1 .. -B $BUILD_DIR > $LOG_FILE
$CMAKE --build $BUILD_DIR --target stellac_runtime_app >> $LOG_FILE
$BUILD_DIR/runtime/stellac_runtime_app "${@:2}"
