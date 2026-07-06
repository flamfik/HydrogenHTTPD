cmake_minimum_required(VERSION 3.20)
project(HydrogenHttpd LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HYDROGENHTTPD_BUILD_TESTS "Build HydrogenHttpd tests" ON)
option(HYDROGENHTTPD_ENABLE_TLS "Build with OpenSSL TLS support" ON)
option(HYDROGENHTTPD_ENABLE_SQLITE "Build SQLite SQL module" ON)

find_package(Boost REQUIRED COMPONENTS system)

set(HYDROGENHTTPD_COMMON_SOURCES
    src/Security.cpp
    src/Config.cpp
    src/Logger.cpp
    src/RateLimiter.cpp
    src/ThreadPool.cpp
    src/Htaccess.cpp
    src/FastCgiClient.cpp
)

if (HYDROGENHTTPD_ENABLE_SQLITE)
    find_package(SQLite3)
    if (SQLite3_FOUND)
        list(APPEND HYDROGENHTTPD_COMMON_SOURCES src/SqliteModule.cpp)
    else()
        message(WARNING "SQLite3 not found; SQL module will be compiled as disabled stub")
        list(APPEND HYDROGENHTTPD_COMMON_SOURCES src/SqliteModuleStub.cpp)
    endif()
else()
    list(APPEND HYDROGENHTTPD_COMMON_SOURCES src/SqliteModuleStub.cpp)
endif()

add_executable(hydrogen_httpd
    src/main.cpp
    src/Server.cpp
    ${HYDROGENHTTPD_COMMON_SOURCES}
)

target_include_directories(hydrogen_httpd PRIVATE src)
target_link_libraries(hydrogen_httpd PRIVATE Boost::system)

if (HYDROGENHTTPD_ENABLE_SQLITE AND SQLite3_FOUND)
    target_compile_definitions(hydrogen_httpd PRIVATE HYDROGENHTTPD_ENABLE_SQLITE=1)
    target_link_libraries(hydrogen_httpd PRIVATE SQLite::SQLite3)
endif()

if (HYDROGENHTTPD_ENABLE_TLS)
    find_package(OpenSSL REQUIRED)
    target_compile_definitions(hydrogen_httpd PRIVATE HYDROGENHTTPD_ENABLE_TLS=1)
    target_link_libraries(hydrogen_httpd PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

if (MSVC)
    target_compile_options(hydrogen_httpd PRIVATE /W4)
else()
    target_compile_options(hydrogen_httpd PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (HYDROGENHTTPD_BUILD_TESTS)
    enable_testing()

    add_executable(test_security tests/test_security.cpp src/Security.cpp)
    target_include_directories(test_security PRIVATE src)

    add_executable(test_config tests/test_config.cpp src/Config.cpp)
    target_include_directories(test_config PRIVATE src)

    add_executable(test_rate_limiter tests/test_rate_limiter.cpp src/RateLimiter.cpp)
    target_include_directories(test_rate_limiter PRIVATE src)

    add_executable(test_thread_pool tests/test_thread_pool.cpp src/ThreadPool.cpp)
    target_include_directories(test_thread_pool PRIVATE src)

    add_executable(test_htaccess tests/test_htaccess.cpp src/Htaccess.cpp)
    target_include_directories(test_htaccess PRIVATE src)

    add_executable(test_fastcgi tests/test_fastcgi.cpp src/FastCgiClient.cpp)
    target_include_directories(test_fastcgi PRIVATE src)
    target_link_libraries(test_fastcgi PRIVATE Boost::system)

    add_test(NAME security_tests COMMAND test_security)
    add_test(NAME config_tests COMMAND test_config)
    add_test(NAME rate_limiter_tests COMMAND test_rate_limiter)
    add_test(NAME thread_pool_tests COMMAND test_thread_pool)
    add_test(NAME htaccess_tests COMMAND test_htaccess)
    add_test(NAME fastcgi_tests COMMAND test_fastcgi)

    if (HYDROGENHTTPD_ENABLE_SQLITE AND SQLite3_FOUND)
        add_executable(test_sqlite tests/test_sqlite.cpp src/SqliteModule.cpp)
        target_include_directories(test_sqlite PRIVATE src)
        target_link_libraries(test_sqlite PRIVATE SQLite::SQLite3)
        target_compile_definitions(test_sqlite PRIVATE HYDROGENHTTPD_ENABLE_SQLITE=1)
        add_test(NAME sqlite_tests COMMAND test_sqlite)
    endif()

    find_package(Python3 COMPONENTS Interpreter)
    if (Python3_FOUND)
        add_test(
            NAME integration_http_tests
            COMMAND
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration_http_tests.py
                $<TARGET_FILE:hydrogen_httpd>
                ${CMAKE_CURRENT_SOURCE_DIR}
        )

        if (HYDROGENHTTPD_ENABLE_TLS)
            add_test(
                NAME integration_https_tests
                COMMAND
                    ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration_https_tests.py
                    $<TARGET_FILE:hydrogen_httpd>
                    ${CMAKE_CURRENT_SOURCE_DIR}
            )
        endif()
    endif()
endif()
