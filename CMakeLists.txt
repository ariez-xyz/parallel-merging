cmake_minimum_required(VERSION 3.5)
project(mpicomm C)

set(CMAKE_C_STANDARD 99)

set(CMAKE_CXX_FLAGS_RELEASE "-O0 -g -rdynamic")
set(CMAKE_C_FLAGS "-O0 -g -rdynamic")
set(CMAKE_C_COMPILER "mpicc")

# For macOS Homebrew-installed lapack
include_directories(/usr/local/opt/lapack/include)
link_directories(/usr/local/opt/lapack/lib)

# same for MPI
include_directories(/usr/local/Cellar/open-mpi/4.0.4/include)
link_directories(/usr/local/opt/libevent/lib)
link_directories(/usr/local/Cellar/open-mpi/4.0.4/lib)

# MPI on Arch...
include_directories(/usr/lib/openmpi)
link_directories(/usr/lib/openmpi)

# Eagle
include_directories(/usr/lib/)
link_directories(/usr/lib/)

# LAPACK on MACH2?
include_directories(/home/d3000/d300342/mpicomm/libraries/lapack-3.9.0/LAPACKE/include)
link_directories(/home/d3000/d300342/mpicomm/libraries)
link_directories(/home/d3000/d300342/mpicomm/libraries/lapack-3.9.0)

add_executable(mpicomm main.c graph.h graph.c lib.h lib.c index.c index.h)

add_executable(test_graph test/test_graph.c graph.h graph.c lib.h lib.c index.c index.h)

add_executable(test_index test/test_index.c graph.h graph.c lib.h lib.c index.c index.h)

add_executable(test_lib test/test_lib.c lib.h lib.c)

add_executable(test_main test/test_main.c main.c graph.h graph.c lib.h lib.c index.c index.h)

target_link_libraries(mpicomm lapacke m)
target_link_libraries(mpicomm lapack m)
target_link_libraries(mpicomm blas m)
target_link_libraries(mpicomm gfortran m)
target_link_libraries(test_graph lapacke m)
target_link_libraries(test_lib lapacke m)
target_link_libraries(test_main lapacke m)
target_link_libraries(test_index lapacke m)

target_link_libraries(mpicomm mpi m)
