cmake_minimum_required(VERSION 3.10)
project(cgull)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)

# finds the ANTLR4 C++ runtime headers
find_path(ANTLR4_INCLUDE_DIR
    NAMES antlr4-runtime.h
    HINTS
        /opt/homebrew/include
        /usr/local/include
        /usr/include
    PATH_SUFFIXES antlr4-runtime
)

# finds the implementation
find_library(ANTLR4_LIBRARY
    NAMES antlr4-runtime
    HINTS
        /opt/homebrew/lib
        /usr/local/lib
        /usr/lib
)

if(NOT ANTLR4_INCLUDE_DIR OR NOT ANTLR4_LIBRARY)
    message(FATAL_ERROR "ANTLR4 library or headers not found. Please install ANTLR4 C++ runtime.")
endif()

message(STATUS "Found ANTLR4 include dir: ${ANTLR4_INCLUDE_DIR}")
message(STATUS "Found ANTLR4 library: ${ANTLR4_LIBRARY}")

# java is needed to generate the antlr4 files
find_package(Java REQUIRED)

set(ANTLR_JAR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/antlr/antlr-4.13.2-complete.jar")
set(GRAMMAR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/grammar")
set(ANTLR_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated-src")

file(MAKE_DIRECTORY ${ANTLR_GENERATED_DIR})

set(GRAMMAR_FILE "${GRAMMAR_DIR}/cgull.g4")

# generate ANTLR4 code
add_custom_command(
    OUTPUT
        ${ANTLR_GENERATED_DIR}/cgullLexer.cpp
        ${ANTLR_GENERATED_DIR}/cgullParser.cpp
        ${ANTLR_GENERATED_DIR}/cgullBaseListener.cpp
        ${ANTLR_GENERATED_DIR}/cgullListener.cpp
        ${ANTLR_GENERATED_DIR}/cgullBaseVisitor.cpp
        ${ANTLR_GENERATED_DIR}/cgullVisitor.cpp
        ${ANTLR_GENERATED_DIR}/cgullLexer.h
        ${ANTLR_GENERATED_DIR}/cgullParser.h
        ${ANTLR_GENERATED_DIR}/cgullBaseListener.h
        ${ANTLR_GENERATED_DIR}/cgullListener.h
        ${ANTLR_GENERATED_DIR}/cgullBaseVisitor.h
        ${ANTLR_GENERATED_DIR}/cgullVisitor.h
    COMMAND
        ${Java_JAVA_EXECUTABLE} -jar ${ANTLR_JAR} -Dlanguage=Cpp -visitor -o ${ANTLR_GENERATED_DIR} ${GRAMMAR_FILE}
    DEPENDS
        ${GRAMMAR_FILE}
    COMMENT "Generating parser from ANTLR grammar"
)

add_custom_target(GenerateParser DEPENDS
    ${ANTLR_GENERATED_DIR}/cgullLexer.cpp
    ${ANTLR_GENERATED_DIR}/cgullParser.cpp
    ${ANTLR_GENERATED_DIR}/cgullBaseListener.cpp
    ${ANTLR_GENERATED_DIR}/cgullListener.cpp
    ${ANTLR_GENERATED_DIR}/cgullBaseVisitor.cpp
    ${ANTLR_GENERATED_DIR}/cgullVisitor.cpp
)

file(GLOB_RECURSE COMPILER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/compiler/*.cpp")
# print the list of files
message(STATUS "Compiler sources: ${COMPILER_SOURCES}")
set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${COMPILER_SOURCES}
    ${ANTLR_GENERATED_DIR}/cgullLexer.cpp
    ${ANTLR_GENERATED_DIR}/cgullParser.cpp
    ${ANTLR_GENERATED_DIR}/cgullBaseListener.cpp
    ${ANTLR_GENERATED_DIR}/cgullListener.cpp
    ${ANTLR_GENERATED_DIR}/cgullBaseVisitor.cpp
)

add_executable(cgull ${SOURCES})

add_dependencies(cgull GenerateParser)

target_link_libraries(cgull ${ANTLR4_LIBRARY})

target_include_directories(cgull PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ANTLR_GENERATED_DIR}
    ${ANTLR4_INCLUDE_DIR}
)
