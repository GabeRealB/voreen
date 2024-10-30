SET(MOD_CORE_MODULECLASS MultiResolutionAggregationModule)

SET(MOD_CORE_SOURCES
    ${MOD_DIR}/utility/memorymappedfile.cpp
)

SET(MOD_CORE_HEADERS
    ${MOD_DIR}/utility/memorymappedfile.h
)

# Deployment
SET(MOD_INSTALL_DIRECTORIES
    ${MOD_DIR}/workspaces
)
