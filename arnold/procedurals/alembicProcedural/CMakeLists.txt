set(PROC arnoldAlembicProcedural)

file(GLOB SRC "*.cpp" "*h" "../../../common/abcshaderutils.h" "../../../common/abcshaderutils.cpp" "../../../common/PathUtil.h" "../../../common/PathUtil.cpp")

add_library(${PROC} SHARED ${SRC})
target_link_libraries(${PROC} ${CONAN_LIBS})
set_target_properties(${PROC} PROPERTIES PREFIX "")

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	install(TARGETS ${PROC} RUNTIME DESTINATION ${DSO_INSTALL_DIR})
else()
	

install(TARGETS ${PROC} DESTINATION ${DSO_INSTALL_DIR})
endif()

