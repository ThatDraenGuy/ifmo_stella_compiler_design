#!/bin/sh
BUILD_DIR=testbuild
CMAKE=cmake
CTEST=ctest
LOG_FILE=testbuild.log

$CMAKE .. -B $BUILD_DIR > $LOG_FILE
$CMAKE --build $BUILD_DIR >> $LOG_FILE
cd $BUILD_DIR
$CTEST -j24 -T test --output-on-failure
