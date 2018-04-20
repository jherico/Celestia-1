include(ExternalProject)
include(SelectLibraryConfigurations)

set(EXTERNAL_NAME cspice)

string(TOUPPER ${EXTERNAL_NAME} EXTERNAL_NAME_UPPER)
ExternalProject_Add(
  ${EXTERNAL_NAME}
  URL http://naif.jpl.nasa.gov/pub/naif/toolkit//C/PC_Windows_VisualC_64bit/packages/cspice.zip
  URL_MD5 68e6495a97338196c704b578a1810c9d
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  LOG_DOWNLOAD 1
)

# Hide this external target (for ide users)
set_target_properties(${EXTERNAL_NAME} PROPERTIES FOLDER "externals")

ExternalProject_Get_Property(${EXTERNAL_NAME} SOURCE_DIR)

set(${EXTERNAL_NAME_UPPER}_INCLUDE_DIRS ${SOURCE_DIR}/include CACHE TYPE INTERNAL)

if (WIN32)
    set(${EXTERNAL_NAME_UPPER}_LIBRARIES ${SOURCE_DIR}/lib/csupport.lib;${SOURCE_DIR}/lib/cspice.lib CACHE TYPE INTERNAL)
elseif(APPLE)
elseif(NOT ANDROID)
endif()


