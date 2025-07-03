cmake_minimum_required(VERSION 3.27)
project(SO2_2)

set(CMAKE_CXX_STANDARD 17)


# Include FetchContent module
include(FetchContent)

# Fetch nlohmann/json
FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.2 # Specify the version
)
FetchContent_MakeAvailable(json)

add_executable(SO2_2 main.cpp)
# Link the json library
target_link_libraries(SO2_2 PRIVATE nlohmann_json::nlohmann_json)