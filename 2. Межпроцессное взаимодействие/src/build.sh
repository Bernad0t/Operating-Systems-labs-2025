#!/bin/bash

# Скрипт сборки для лабораторной работы по межпроцессному взаимодействию
# Очищает все промежуточные файлы cmake и собирает проект

set -e  # Прерывать выполнение при ошибке

echo "Cleaning build directory..."
rm -rf CMakeFiles
rm -rf CMakeCache.txt
rm -rf cmake_install.cmake
rm -rf Makefile
rm -rf *.o
rm -rf *.a
rm -rf host_*
rm -rf client_*

echo "Running cmake..."
cmake .

echo "Building project..."
make

echo "Build completed successfully!"
echo "Executables created:"
ls -1 host_* client_* 2>/dev/null || echo "No executables found"

