#! /bin/bash

function build_cxx () {
  rm -rf  "build-linux/cxx_$1"
  cmake -S . -B "build-linux/cxx_$1" -DCMAKE_CXX_STANDARD=$1 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_CXX_EXTENSIONS=OFF || return 1
  cmake --build "build-linux/cxx_$1" --clean-first -v -j || return 1
  return 0
}

build_cxx 23 || exit 1
build_cxx 20 || exit 1
build_cxx 17 || exit 1
build_cxx 14 || exit 1
build_cxx 11 || exit 1

exit 0
