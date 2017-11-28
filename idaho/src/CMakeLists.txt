set(LIB_NAME ossim_idaho_plugin)
MESSAGE( "************** LIBRARY SETUP FOR ossim_idaho_plugin ******************")

# Include ossim cmake stuff:
include(OssimVersion)
include(OssimCommonVariables)
include(OssimUtilities)

set(requiredLibs)

# OSSIM - Required: 
find_package(ossim)
if(OSSIM_FOUND)
   include_directories( ${OSSIM_INCLUDE_DIR} )
   set( requiredLibs ${requiredLibs} ${OSSIM_LIBRARIES} )
else(OSSIM_FOUND)
   message(FATAL_ERROR "Could not find ossim")
endif(OSSIM_FOUND)

# BOOST - Required: 
find_package(Boost)
if(Boost_FOUND)
   include_directories( ${Boost_INCLUDE_DIR} )
   set( requiredLibs ${requiredLibs} ${Boost_LIBRARIES} )
else(Boost_FOUND)
   message(FATAL_ERROR "Could not find Boost")
endif(Boost_FOUND)


find_package(IDAHO)
if(IDAHO_FOUND)
    include_directories( ${IDAHO_INCLUDE_DIR} )
    set( requiredLibs ${requiredLibs} ${IDAHO_LIBRARIES} )
else(IDAHO_FOUND)
    message(FATAL_ERROR "Could not find IDAHO")
endif(IDAHO_FOUND)


####
# OSSIMPLUGINSMAKINGDLL controls dll linkage on windows.  
# Adding this sets OSSIM_PLUGINS_DLL #define TO "__declspec(dllexport)".
###
add_definitions("-DOSSIMPLUGINSMAKINGDLL")

FILE(GLOB_RECURSE OSSIMPLUGIN_SRCS *.cpp)
FILE(GLOB_RECURSE OSSIMPLUGIN_HEADERS *.h)

# Adjust the install library path:
if(OSSIM_INSTALL_PLUGINS_WITH_VERSION)
   set(INSTALL_LIBRARY_DIR   "${INSTALL_LIBRARY_DIR}/ossim-${OSSIM_VERSION}/plugins")
else(OSSIM_INSTALL_PLUGINS_WITH_VERSION)
   set(INSTALL_LIBRARY_DIR   "${INSTALL_LIBRARY_DIR}/ossim/plugins")
endif(OSSIM_INSTALL_PLUGINS_WITH_VERSION)
if(WIN32)
 set(INSTALL_RUNTIME_DIR "${INSTALL_RUNTIME_DIR}/ossim/plugins")
endif()

OSSIM_LINK_LIBRARY(${LIB_NAME}
                   COMPONENT_NAME ossim TYPE "${OSSIM_PLUGIN_LINK_TYPE}"
		   LIBRARIES ${requiredLibs} 
                   HEADERS "${OSSIMPLUGIN_HEADERS}" "${IDAHO_INCLUDE_DIR}"
		   SOURCE_FILES "${OSSIMPLUGIN_SRCS}"
                   INSTALL_LIB)

