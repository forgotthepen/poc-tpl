cmake_minimum_required(VERSION 3.16)

set(project_name "poc-tpl-test")

project(${project_name})

file(GLOB_RECURSE src_files
    "test/*.cpp"
    "observable/*.hpp"
    "libs/observable-queue/observable/*.hpp"
)

add_executable(${project_name}
    ${src_files}
)

target_include_directories(${project_name} PUBLIC
    "./"
    "libs/observable-queue/"
)

set_target_properties(${project_name} PROPERTIES
    LINKER_LANGUAGE CXX
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
