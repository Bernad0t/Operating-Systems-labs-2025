#!/bin/bash
set -e

CXX=g++
CXXFLAGS="-std=c++17 -Wall -Werror"
OUT=daemon15

$CXX $CXXFLAGS sources/*.cpp -o $OUT