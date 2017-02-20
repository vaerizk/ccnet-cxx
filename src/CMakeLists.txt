find_package(Boost 1.35.0 REQUIRED)

set(CCNET_PRIVATE_HEADERS
	utility.h
)
set(CCNET_PUBLIC_HEADERS
	${PROJECT_SOURCE_DIR}/include/${CCNET_TARGET_NAME}/bill_validator.h
	${PROJECT_SOURCE_DIR}/include/${CCNET_TARGET_NAME}/cash_type.h
	${PROJECT_SOURCE_DIR}/include/${CCNET_TARGET_NAME}/ccnet.h
)
set(CCNET_SOURCES
	bill_validator.cpp
	cash_type.cpp
	utility.cpp
)

add_library(${CCNET_TARGET_NAME} STATIC
	${CCNET_PRIVATE_HEADERS}
	${CCNET_PUBLIC_HEADERS}
	${CCNET_SOURCES}
)

target_include_directories(${CCNET_TARGET_NAME}
	PRIVATE
		${Boost_INCLUDE_DIRS}
		${PROJECT_SOURCE_DIR}/include/${CCNET_TARGET_NAME}
	INTERFACE
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
)

target_link_libraries(${CCNET_TARGET_NAME}
	${Boost_LIBRARIES}
)

install(TARGETS ${CCNET_TARGET_NAME}
	EXPORT ${CCNET_EXPORT_NAME}
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)

# group source files for IDE source explorers (e.g. Visual Studio)
source_group("Private Header Files" FILES ${CCNET_PRIVATE_HEADERS})
source_group("Public Header Files" FILES ${CCNET_PUBLIC_HEADERS})
source_group("Source Files" FILES ${CCNET_SOURCES})
