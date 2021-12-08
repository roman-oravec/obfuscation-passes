cmake_minimum_required(VERSION 3.1)
project(obfuscator)

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(llvm-pass-mba)
add_subdirectory(llvm-pass-bogus)  
add_subdirectory(llvm-pass-obfconst)
add_subdirectory(llvm-pass-obfstring)
