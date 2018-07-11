
################################################################################
# External dependency: Python library
################################################################################

IF(WIN32)
    SET(VRN_USE_PYTHON_VERSION python37)
    SET(MOD_DEFINITIONS "-DVRN_USE_PYTHON_VERSION=\"${VRN_USE_PYTHON_VERSION}\"")

    SET(MOD_INCLUDE_DIRECTORIES "${MOD_DIR}/ext/${VRN_USE_PYTHON_VERSION}/include")

	SET(MOD_RELEASE_DLLS
		"${MOD_DIR}/ext/${VRN_USE_PYTHON_VERSION}/${VRN_USE_PYTHON_VERSION}.dll"
	)
    SET(MOD_DEBUG_DLLS
		"${MOD_DIR}/ext/${VRN_USE_PYTHON_VERSION}/${VRN_USE_PYTHON_VERSION}_d.dll"
	)
	SET(MOD_RELEASE_LIBRARIES
		"${MOD_DIR}/ext/${VRN_USE_PYTHON_VERSION}/libs/${VRN_USE_PYTHON_VERSION}.lib"
	)
    SET(MOD_DEBUG_LIBRARIES
		"${MOD_DIR}/ext/${VRN_USE_PYTHON_VERSION}/libs/${VRN_USE_PYTHON_VERSION}_d.lib"
	)
    
    # deployment
    SET(MOD_INSTALL_DIRECTORIES
        ${MOD_DIR}/ext/${VRN_USE_PYTHON_VERSION}/lib
        ${MOD_DIR}/scripts
    )
    SET(MOD_INSTALL_FILES
        ${MOD_DIR}/ext/${VRN_USE_PYTHON_VERSION}/LICENSE.txt
    )

ELSEIF(UNIX)
    MESSAGE(STATUS "Trying to find Python 3 version...")
    FIND_PACKAGE(PythonLibs "3")
    IF(PYTHONLIBS_FOUND)
        MESSAGE(STATUS "  - Found Python library")
        SET(MOD_INCLUDE_DIRECTORIES ${PYTHON_INCLUDE_DIRS})
        SET(MOD_LIBRARIES ${PYTHON_LIBRARIES})
    ELSE()
        MESSAGE(FATAL_ERROR "Python library not found!")
    ENDIF()
ENDIF()


################################################################################
# Core module resources 
################################################################################
SET(MOD_CORE_MODULECLASS PythonModule)

SET(MOD_CORE_SOURCES 
    ${MOD_DIR}/core/pythonscript.cpp
    ${MOD_DIR}/core/pyvoreen.cpp
)

SET(MOD_CORE_HEADERS
    ${MOD_DIR}/core/pythonscript.h
    ${MOD_DIR}/core/pyvoreen.h
)


################################################################################
# Qt module resources 
################################################################################
SET(MOD_QT_MODULECLASS PythonModuleQt)

SET(MOD_QT_SOURCES 
    ${MOD_DIR}/qt/pyvoreenqt.cpp
    ${MOD_DIR}/qt/pythonhighlighter.cpp
    ${MOD_DIR}/qt/menuentity/pythoneditor.cpp
)

SET(MOD_QT_HEADERS
    ${MOD_DIR}/qt/menuentity/pythoneditor.h
)

SET(MOD_QT_HEADERS_NONMOC
    ${MOD_DIR}/qt/pyvoreenqt.h
    ${MOD_DIR}/qt/pythonhighlighter.h
)

SET(MOD_QT_RESOURCES
    ${MOD_DIR}/qt/menuentity/python.qrc
)
