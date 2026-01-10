cmake_minimum_required(VERSION 3.10)
project(ChatIPC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

# Общие исходные файлы
set(COMMON_SOURCES
    conn_base.cpp
    common.cpp
)

# Источники для каждого типа соединения
set(MMAP_SOURCES
    ${COMMON_SOURCES}
    conn_mmap.cpp
)

set(SHM_SOURCES
    ${COMMON_SOURCES}
    conn_shm.cpp
)

set(FIFO_SOURCES
    ${COMMON_SOURCES}
    conn_fifo.cpp
)

# Создание исполняемых файлов host_*
add_executable(host_mmap host.cpp ${MMAP_SOURCES})
target_link_libraries(host_mmap pthread rt)

add_executable(host_shm host.cpp ${SHM_SOURCES})
target_link_libraries(host_shm pthread rt)

add_executable(host_fifo host.cpp ${FIFO_SOURCES})
target_link_libraries(host_fifo pthread rt)

# Создание исполняемых файлов client_*
add_executable(client_mmap client.cpp ${MMAP_SOURCES})
target_link_libraries(client_mmap pthread rt)

add_executable(client_shm client.cpp ${SHM_SOURCES})
target_link_libraries(client_shm pthread rt)

add_executable(client_fifo client.cpp ${FIFO_SOURCES})
target_link_libraries(client_fifo pthread rt)

