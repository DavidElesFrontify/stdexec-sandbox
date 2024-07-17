project(STDEXEC_SANDBOX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(3rd-party/stdexec-src)

add_executable(sandbox
    main.cpp
    #main_multi.cpp
    #main_multi_let.cpp
    #main_coro.cpp
    Input.cpp Output.cpp Transformator.cpp Image.cpp)

target_link_libraries(sandbox PUBLIC STDEXEC::stdexec)
