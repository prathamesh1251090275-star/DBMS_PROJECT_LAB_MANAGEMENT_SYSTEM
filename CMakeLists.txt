cmake_minimum_required(VERSION 3.16)
project(LabEquipmentSystem CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon CONFIG REQUIRED)

# SQLite and libsodium: vcpkg provides CMake packages ("unofficial-..."); Linux uses pkg-config.
# (We do not call find_package(SQLite3) because Drogon ships its own module with that name.)
find_package(unofficial-sqlite3 CONFIG QUIET)
find_package(unofficial-sodium CONFIG QUIET)
if(NOT unofficial-sqlite3_FOUND OR NOT unofficial-sodium_FOUND)
  find_package(PkgConfig REQUIRED)
endif()
if(unofficial-sqlite3_FOUND)
  set(SQLITE_TARGET unofficial::sqlite3::sqlite3)
else()
  pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)
  set(SQLITE_TARGET PkgConfig::SQLITE3)
endif()
if(unofficial-sodium_FOUND)
  set(SODIUM_TARGET unofficial-sodium::sodium)
else()
  pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
  set(SODIUM_TARGET PkgConfig::SODIUM)
endif()

add_executable(lab_server
  main.cpp
  config/Config.cpp
  database/Db.cpp
  services/Password.cpp
  services/IssueService.cpp
  services/MaintenanceService.cpp
  services/EquipmentService.cpp
  controllers/Common.cpp
  controllers/AuthController.cpp
  controllers/EquipmentController.cpp
  controllers/LookupController.cpp
  controllers/IssueController.cpp
  controllers/MaintenanceController.cpp
  controllers/ReportController.cpp)

target_link_libraries(lab_server PRIVATE Drogon::Drogon ${SQLITE_TARGET} ${SODIUM_TARGET})
