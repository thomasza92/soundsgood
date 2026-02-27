cmake_minimum_required(VERSION 3.15)

PROJECT(JUCE_CMAKE_EXAMPLE
LANGUAGES C CXX
VERSION 0.0.2
)
# link with homebrew llvm over system clang on macOS
if (APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(HOMEBREW_LLVM_PREFIX "/opt/homebrew/opt/llvm")
  set(HOMEBREW_LIBCXX_DIR "${HOMEBREW_LLVM_PREFIX}/lib/c++")
  add_link_options(
    "-L${HOMEBREW_LIBCXX_DIR}"
    "-Wl,-rpath,${HOMEBREW_LIBCXX_DIR}"
  )
endif()

# for clang-tidy(this enable to find system header files).
if(CMAKE_EXPORT_COMPILE_COMMANDS)
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

# Enable JUCE. Do not use find_package to prevent from mix up with one globally installed.
add_subdirectory(lib/JUCE)

add_subdirectory(src)