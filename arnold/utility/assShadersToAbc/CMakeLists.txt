set(UTIL assShadersToAbc)

file(GLOB SRC "*.cpp" "*h" "../../../common/abc*.h" "../../../common/abc*.cpp")

add_executable(${UTIL} ${SRC})
target_link_libraries(${UTIL} ${CONAN_LIBS})
set_target_properties(${UTIL} PROPERTIES PREFIX "")

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	install(TARGETS ${UTIL} RUNTIME DESTINATION ${DSO_INSTALL_DIR})
else()
	

install(TARGETS ${UTIL} DESTINATION ${DSO_INSTALL_DIR})
endif()

