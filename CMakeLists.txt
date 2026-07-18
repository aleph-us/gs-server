cmake_minimum_required(VERSION 3.16)

project(GSServer LANGUAGES C CXX)

# Ghostscript is AGPL — system package only, never bundled.
# Build machine and production server need the same version installed.
# Use `dpkg -l ghostscript` on prod to check, then pin in CI:
#   apt-get install "ghostscript=<ver>" "libgs-dev=<ver>"
find_library(GS_LIB NAMES gs REQUIRED)

# SQL (ODBC) log-channel support: the sql channel in GSServer.properties
# writes the log stream to a database table. OFF drops the Poco Data/ODBC
# dependency entirely (remove the sql channel from the logging config then).
option(GSSERVER_ENABLE_SQL_LOGGING "Database (ODBC) log channel support" ON)

set(GSSERVER_POCO_COMPONENTS Foundation XML JSON Util Net)
if(GSSERVER_ENABLE_SQL_LOGGING)
    list(APPEND GSSERVER_POCO_COMPONENTS Data DataODBC)
endif()

# Standalone builds locate an installed Poco; a build that embeds this
# project provides the Poco targets itself.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT TARGET Poco::Foundation)
    find_package(Poco REQUIRED CONFIG COMPONENTS ${GSSERVER_POCO_COMPONENTS})
endif()

# Poco::DataODBC's imported target links ODBC::ODBC; provide it unless the
# embedding build already has.
if(GSSERVER_ENABLE_SQL_LOGGING AND NOT TARGET ODBC::ODBC)
    find_package(ODBC REQUIRED)
endif()

add_executable(GSServer
    src/GSServerApp.cpp
    src/GSHTTPTask.cpp
    src/GSWorkerTask.cpp
    src/GSSenderTask.cpp)

target_compile_features(GSServer PRIVATE cxx_std_20)

target_include_directories(GSServer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(GSServer PRIVATE
    Poco::Util Poco::Net Poco::XML Poco::JSON Poco::Foundation
    ${GS_LIB})

if(GSSERVER_ENABLE_SQL_LOGGING)
    target_compile_definitions(GSServer PRIVATE GSSERVER_ENABLE_SQL_LOGGING)
    target_link_libraries(GSServer PRIVATE Poco::DataODBC Poco::Data)
endif()

# Stage the default config next to the binary so the build tree is runnable.
add_custom_command(TARGET GSServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/GSServer.properties"
        "$<TARGET_FILE_DIR:GSServer>/GSServer.properties"
    COMMENT "Copying GSServer.properties")

# Install only when built as the top-level project; an embedding build
# defines its own install layout.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    install(TARGETS GSServer RUNTIME DESTINATION bin)
    install(FILES GSServer.properties DESTINATION bin)
endif()
