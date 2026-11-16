cmake_minimum_required(VERSION 3.20)
project(mycomplier LANGUAGES CXX)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(MYCOMPLIER_PROFILING "Build with gprof (-pg)" OFF)
if(MYCOMPLIER_PROFILING)
  add_compile_options(-pg)
  add_link_options(-pg)
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_program(ANTLR_EXECUTABLE NAMES antlr4 REQUIRED)
find_package(antlr4-runtime CONFIG QUIET)
if(NOT antlr4-runtime_FOUND)
  # Fallback for installations that ship headers/libraries without a CMake
  # package config (e.g. a manually installed antlr4-runtime).
  find_path(ANTLR4_INCLUDE_DIR NAMES antlr4-runtime.h PATH_SUFFIXES antlr4-runtime)
  find_library(ANTLR4_LIBRARY NAMES antlr4-runtime libantlr4-runtime)
  if(NOT ANTLR4_INCLUDE_DIR OR NOT ANTLR4_LIBRARY)
    message(FATAL_ERROR
      "antlr4-runtime not found. Install it, set CMAKE_PREFIX_PATH, or pass "
      "ANTLR4_INCLUDE_DIR and ANTLR4_LIBRARY.")
  endif()
  add_library(mycomplier_antlr4 UNKNOWN IMPORTED)
  set_target_properties(mycomplier_antlr4 PROPERTIES
    IMPORTED_LOCATION "${ANTLR4_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${ANTLR4_INCLUDE_DIR}"
  )
endif()
find_package(Boost REQUIRED COMPONENTS program_options)
find_package(Threads REQUIRED)

message(STATUS "ANTLR tool:            ${ANTLR_EXECUTABLE}")
message(STATUS "ANTLR runtime include: ${ANTLR4_INCLUDE_DIR}")

# ---------------------------------------------------------------------------
# Generate the parser out-of-source.  This keeps the checked-in generated/
# directory (used by the xmake build) untouched and guarantees that the
# generated code matches the ANTLR runtime found above.
# ---------------------------------------------------------------------------
set(GRAMMAR_FILES
  ${CMAKE_SOURCE_DIR}/grammar/CLexer.g4
  ${CMAKE_SOURCE_DIR}/grammar/CParser.g4
)
set(ANTLR_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${ANTLR_GENERATED_DIR})

set(ANTLR_GENERATED_SOURCES
  ${ANTLR_GENERATED_DIR}/CLexer.cpp
  ${ANTLR_GENERATED_DIR}/CParser.cpp
  ${ANTLR_GENERATED_DIR}/CParserBaseListener.cpp
  ${ANTLR_GENERATED_DIR}/CParserBaseVisitor.cpp
  ${ANTLR_GENERATED_DIR}/CParserListener.cpp
  ${ANTLR_GENERATED_DIR}/CParserVisitor.cpp
)

add_custom_command(
  OUTPUT ${ANTLR_GENERATED_SOURCES}
  COMMAND ${ANTLR_EXECUTABLE} -Dlanguage=Cpp -visitor -listener
          -Xexact-output-dir -o ${ANTLR_GENERATED_DIR} ${GRAMMAR_FILES}
  DEPENDS ${GRAMMAR_FILES} ${ANTLR_EXECUTABLE}
  COMMENT "Generating C parser with ANTLR"
  VERBATIM
)

# ---------------------------------------------------------------------------
# Target
# ---------------------------------------------------------------------------
file(GLOB_RECURSE MYCOMPLIER_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")

set(CPP_GRAMMAR_SOURCES
  ${CMAKE_SOURCE_DIR}/grammar/Cpp/CLexerBase.cpp
  ${CMAKE_SOURCE_DIR}/grammar/Cpp/CParserBase.cpp
  ${CMAKE_SOURCE_DIR}/grammar/Cpp/SymbolTable.cpp
)

add_executable(mycomplier
  ${MYCOMPLIER_SOURCES}
  ${CPP_GRAMMAR_SOURCES}
  ${ANTLR_GENERATED_SOURCES}
)

target_include_directories(mycomplier PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/grammar/Cpp
  ${CMAKE_SOURCE_DIR}/third_party/magic_enum/include
  ${ANTLR_GENERATED_DIR}
  ${ANTLR4_INCLUDE_DIR}
)

# The source tree is the compiler's "installation root" for stdhead/ and
# stdlibc/; pass it in so the binary does not depend on the current directory.
target_compile_definitions(mycomplier PRIVATE
  MYCOMPLIER_ROOT="${CMAKE_SOURCE_DIR}"
)

if(TARGET antlr4_static)
  set(ANTLR4_RUNTIME_TARGET antlr4_static)
elseif(TARGET antlr4_shared)
  set(ANTLR4_RUNTIME_TARGET antlr4_shared)
elseif(TARGET mycomplier_antlr4)
  set(ANTLR4_RUNTIME_TARGET mycomplier_antlr4)
else()
  set(ANTLR4_RUNTIME_TARGET antlr4-runtime)
endif()

target_link_libraries(mycomplier PRIVATE
  ${ANTLR4_RUNTIME_TARGET}
  Boost::program_options
  Threads::Threads
)

# ---------------------------------------------------------------------------
# magic_enum is vendored under third_party/magic_enum (MIT) so configuration
# works offline; nothing to fetch or link.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(${CMAKE_SOURCE_DIR}/test)
