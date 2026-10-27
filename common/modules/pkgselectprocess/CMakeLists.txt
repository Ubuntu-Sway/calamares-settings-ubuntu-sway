cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

include(FeatureSummary)

set( CMAKE_CXX_STANDARD 23 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

find_library(APT_PKG_LIB apt-pkg)
if (NOT APT_PKG_LIB)
    message(FATAL_ERROR "Could not find libapt-pkg")
endif()

find_package(ECM "6.0.0" NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_MODULE_PATH})
find_package(KF6 REQUIRED COMPONENTS CoreAddons)
include(KDEInstallDirs)

set( CALAMARES_VERSION_REQUIRED 3.3.9 )
find_package(Calamares ${CALAMARES_VERSION_REQUIRED} NO_CMAKE_PACKAGE_REGISTRY)
if (NOT TARGET Calamares::calamares OR NOT TARGET Calamares::calamaresui)
    find_package(Calamares ${CALAMARES_VERSION_REQUIRED} REQUIRED)
endif()

message(STATUS "Found Calamares version ${Calamares_VERSION}")
message(STATUS "              libraries ${Calamares_LIB_DIRS}")
message(STATUS "")

calamares_add_plugin( pkgselectprocess
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
    	PackageSelectProcess.cpp
    SHARED_LIB
    NO_CONFIG
)
add_executable(check_package checkpackage-backend.cpp)
target_link_libraries(check_package PRIVATE ${APT_PKG_LIB})
