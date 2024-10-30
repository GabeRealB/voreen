SET(MOD_CORE_MODULECLASS MultiResolutionAggregationModule)

SET(MOD_CORE_SOURCES
    ${MOD_DIR}/utility/field.cpp
    ${MOD_DIR}/utility/memorymappedfile.cpp
    ${MOD_DIR}/utility/utility.cpp
)

SET(MOD_CORE_HEADERS
    ${MOD_DIR}/utility/field.h
    ${MOD_DIR}/utility/memorymappedfile.h
    ${MOD_DIR}/utility/utility.h
)

# Deployment
SET(MOD_INSTALL_DIRECTORIES
    ${MOD_DIR}/workspaces
)
