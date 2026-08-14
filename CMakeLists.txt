cmake_minimum_required(VERSION 3.20)
project(HydrogenHttpd VERSION 1.9.0 LANGUAGES CXX)

if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HYDROGENHTTPD_BUILD_TESTS "Build HydrogenHttpd tests" ON)
option(HYDROGENHTTPD_ENABLE_TLS "Build with OpenSSL TLS support" ON)
option(HYDROGENHTTPD_ENABLE_SQLITE "Build SQLite SQL module" ON)
option(HYDROGENHTTPD_WARNINGS_AS_ERRORS "Treat project compiler warnings as errors" OFF)

find_package(Boost REQUIRED)

set(HYDROGENHTTPD_COMMON_SOURCES
    src/Auth.cpp
    src/AuthRuntimeStore.cpp
    src/Security.cpp
    src/Config.cpp
    src/Logger.cpp
    src/StaticFileCache.cpp
    src/RateLimiter.cpp
    src/ThreadPool.cpp
    src/Htaccess.cpp
    src/StrictHttp.cpp
    src/Multipart.cpp
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
    src/Application.cpp
    src/Server.cpp
    ${HYDROGENHTTPD_COMMON_SOURCES}
)

if (WIN32)
    target_sources(hydrogen_httpd PRIVATE src/WindowsService.cpp)
    target_link_libraries(hydrogen_httpd PRIVATE advapi32)
endif()

target_include_directories(hydrogen_httpd PRIVATE src)
target_compile_definitions(hydrogen_httpd PRIVATE BOOST_ERROR_CODE_HEADER_ONLY=1 BOOST_SYSTEM_NO_DEPRECATED=1 HYDROGENHTTPD_VERSION="${PROJECT_VERSION}")
if (TARGET Boost::headers)
    target_link_libraries(hydrogen_httpd PRIVATE Boost::headers)
elseif (TARGET Boost::boost)
    target_link_libraries(hydrogen_httpd PRIVATE Boost::boost)
endif()

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
    if (HYDROGENHTTPD_WARNINGS_AS_ERRORS)
        target_compile_options(hydrogen_httpd PRIVATE -Werror)
    endif()
endif()

if (HYDROGENHTTPD_BUILD_TESTS)
    enable_testing()

    add_executable(test_auth tests/test_auth.cpp src/Auth.cpp)
    target_include_directories(test_auth PRIVATE src)

    add_executable(test_auth_runtime_store tests/test_auth_runtime_store.cpp src/Auth.cpp src/AuthRuntimeStore.cpp)
    target_include_directories(test_auth_runtime_store PRIVATE src)

    add_executable(test_logger tests/test_logger.cpp src/Logger.cpp)
    target_include_directories(test_logger PRIVATE src)

    add_executable(test_static_file_cache tests/test_static_file_cache.cpp src/StaticFileCache.cpp)
    target_include_directories(test_static_file_cache PRIVATE src)

    add_executable(test_security tests/test_security.cpp src/Security.cpp)
    target_include_directories(test_security PRIVATE src)

    add_executable(test_config tests/test_config.cpp src/Config.cpp src/Auth.cpp)
    target_include_directories(test_config PRIVATE src)

    add_executable(test_rate_limiter tests/test_rate_limiter.cpp src/RateLimiter.cpp)
    target_include_directories(test_rate_limiter PRIVATE src)

    add_executable(test_thread_pool tests/test_thread_pool.cpp src/ThreadPool.cpp)
    target_include_directories(test_thread_pool PRIVATE src)

    add_executable(test_htaccess tests/test_htaccess.cpp src/Htaccess.cpp)
    target_include_directories(test_htaccess PRIVATE src)


    add_executable(test_strict_http tests/test_strict_http.cpp src/StrictHttp.cpp)
    target_include_directories(test_strict_http PRIVATE src)

    add_executable(test_multipart tests/test_multipart.cpp src/Multipart.cpp)
    target_include_directories(test_multipart PRIVATE src)

    add_executable(test_fastcgi tests/test_fastcgi.cpp src/FastCgiClient.cpp)
    target_include_directories(test_fastcgi PRIVATE src)
    target_compile_definitions(test_fastcgi PRIVATE BOOST_ERROR_CODE_HEADER_ONLY=1 BOOST_SYSTEM_NO_DEPRECATED=1)
    if (TARGET Boost::headers)
        target_link_libraries(test_fastcgi PRIVATE Boost::headers)
    elseif (TARGET Boost::boost)
        target_link_libraries(test_fastcgi PRIVATE Boost::boost)
    endif()

    add_test(NAME auth_tests COMMAND test_auth)
    add_test(NAME auth_runtime_store_tests COMMAND test_auth_runtime_store)
    add_test(NAME logger_tests COMMAND test_logger)
    add_test(NAME static_file_cache_tests COMMAND test_static_file_cache)
    add_test(NAME security_tests COMMAND test_security)
    add_test(NAME config_tests COMMAND test_config)
    add_test(NAME rate_limiter_tests COMMAND test_rate_limiter)
    add_test(NAME thread_pool_tests COMMAND test_thread_pool)
    add_test(NAME htaccess_tests COMMAND test_htaccess)
    add_test(NAME strict_http_tests COMMAND test_strict_http)
    add_test(NAME multipart_tests COMMAND test_multipart)
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


        add_test(
            NAME exploit_regression_tests
            COMMAND
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration_exploit_regression_tests.py
                $<TARGET_FILE:hydrogen_httpd>
        )

        add_test(
            NAME integration_post_body_tests
            COMMAND
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration_post_body_tests.py
                $<TARGET_FILE:hydrogen_httpd>
        )

        add_test(
            NAME integration_upload_tests
            COMMAND
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration_upload_tests.py
                $<TARGET_FILE:hydrogen_httpd>
        )

        add_test(
            NAME integration_upload_auth_tests
            COMMAND
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration_upload_auth_tests.py
                $<TARGET_FILE:hydrogen_httpd>
        )

        add_test(
            NAME integration_admin_status_tests
            COMMAND
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration_admin_status_tests.py
                $<TARGET_FILE:hydrogen_httpd>
        )

        add_test(
            NAME integration_auth_hot_reload_tests
            COMMAND
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration_auth_hot_reload_tests.py
                $<TARGET_FILE:hydrogen_httpd>
        )
    endif()
endif()

if (HYDROGENHTTPD_BUILD_TESTS)
    set_tests_properties(auth_tests auth_runtime_store_tests logger_tests static_file_cache_tests security_tests config_tests rate_limiter_tests thread_pool_tests htaccess_tests strict_http_tests multipart_tests fastcgi_tests PROPERTIES TIMEOUT 20)
    if (TEST sqlite_tests)
        set_tests_properties(sqlite_tests PROPERTIES TIMEOUT 20)
    endif()
    if (TEST integration_http_tests)
        set_tests_properties(integration_http_tests PROPERTIES TIMEOUT 30)
    endif()
    if (TEST integration_https_tests)
        set_tests_properties(integration_https_tests PROPERTIES TIMEOUT 30)
    endif()
    if (TEST exploit_regression_tests)
        set_tests_properties(exploit_regression_tests PROPERTIES TIMEOUT 30)
    endif()
    if (TEST integration_post_body_tests)
        set_tests_properties(integration_post_body_tests PROPERTIES TIMEOUT 30)
    endif()
    if (TEST integration_upload_tests)
        set_tests_properties(integration_upload_tests PROPERTIES TIMEOUT 30)
    endif()
    if (TEST integration_upload_auth_tests)
        set_tests_properties(integration_upload_auth_tests PROPERTIES TIMEOUT 30)
    endif()
    if (TEST integration_admin_status_tests)
        set_tests_properties(integration_admin_status_tests PROPERTIES TIMEOUT 30)
    endif()
    if (TEST integration_auth_hot_reload_tests)
        set_tests_properties(integration_auth_hot_reload_tests PROPERTIES TIMEOUT 40)
    endif()
endif()


include(GNUInstallDirs)

install(
    TARGETS hydrogen_httpd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    FILES
        README.md
        LICENSE
        SECURITY.md
        CHANGELOG.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/hydrogenhttpd
)

install(
    DIRECTORY docs/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/hydrogenhttpd/docs
)

install(
    FILES www/index.html
    DESTINATION ${CMAKE_INSTALL_DATADIR}/hydrogenhttpd/www
)

install(
    FILES
        secrets/auth.tokens.example
    DESTINATION ${CMAKE_INSTALL_DATADIR}/hydrogenhttpd/secrets
)

install(
    PROGRAMS
        scripts/generate_auth_token.py
        scripts/manage_auth_tokens.py
    DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/hydrogenhttpd
)

if (WIN32)
    install(
        FILES packaging/windows/server.conf
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hydrogenhttpd/config
    )
    install(
        FILES
            packaging/windows/install-service.ps1
            packaging/windows/uninstall-service.ps1
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hydrogenhttpd/windows
    )
elseif (APPLE)
    install(
        FILES packaging/macos/server.conf
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hydrogenhttpd/config
    )
    install(
        PROGRAMS
            packaging/macos/install.sh
            packaging/macos/uninstall.sh
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hydrogenhttpd/macos
    )
    install(
        FILES packaging/macos/com.hydrogenhttpd.server.plist
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hydrogenhttpd/macos
    )
else()
    install(
        FILES packaging/linux/server.conf
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hydrogenhttpd/config
    )
    install(
        FILES packaging/linux/hydrogenhttpd.service
        DESTINATION lib/systemd/system
    )
    install(
        FILES packaging/linux/hydrogenhttpd.logrotate
        DESTINATION /etc/logrotate.d
        RENAME hydrogenhttpd
    )
    install(
        FILES packaging/linux/hydrogenhttpd.tmpfiles
        DESTINATION lib/tmpfiles.d
        RENAME hydrogenhttpd.conf
    )
    install(
        PROGRAMS
            packaging/linux/install.sh
            packaging/linux/uninstall.sh
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/hydrogenhttpd
    )
endif()

set(CPACK_PACKAGE_NAME "HydrogenHttpd")
set(CPACK_PACKAGE_VENDOR "HydrogenHttpd Project")
set(CPACK_PACKAGE_CONTACT "post.apo@gmail.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance security-focused HTTP/HTTPS server")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/flamfik/hydrogenhttpd")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_FILE_NAME "HydrogenHttpd-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_STRIP_FILES ON)
set(CPACK_SET_DESTDIR ON)

if (WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif (APPLE)
    set(CPACK_GENERATOR "TGZ")
else()
    set(CPACK_GENERATOR "DEB;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "HydrogenHttpd Project")
    set(CPACK_DEBIAN_PACKAGE_SECTION "web")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/debian/postinst"
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/debian/prerm"
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/debian/postrm"
    )
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "System Environment/Daemons")
    set(CPACK_RPM_PACKAGE_AUTOREQ ON)
    set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/rpm/postinst")
    set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/rpm/preun")
    set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/rpm/postun")
endif()

include(CPack)
