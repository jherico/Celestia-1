set(EXTERNAL_NAME Eigen)
string(TOUPPER ${EXTERNAL_NAME} EXTERNAL_NAME_UPPER)

include(ExternalProject)

ExternalProject_Add(
    ${EXTERNAL_NAME}
    URL http://bitbucket.org/eigen/eigen/get/3.3.4.tar.gz
    URL_MD5 1a47e78efe365a97de0c022d127607c3
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
    BINARY_DIR ${EXTERNAL_PROJECT_PREFIX}/build
    LOG_DOWNLOAD 1
    LOG_CONFIGURE 1
    LOG_BUILD 1
)


# Hide this external target (for ide users)
set_target_properties(${EXTERNAL_NAME} PROPERTIES FOLDER "externals")
ExternalProject_Get_Property(${EXTERNAL_NAME} INSTALL_DIR)
set(${EXTERNAL_NAME_UPPER}_INCLUDE_DIRS ${INSTALL_DIR}/include/eigen3 CACHE TYPE INTERNAL)
