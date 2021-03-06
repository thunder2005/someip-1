# Add a doxygen documentation to the targets to be built and installed.
# The packageName parameter defines the name of the package
# The sourceCodePath parameter defines the path to the files to include in the documentation. Subdirectories are also considered as source paths.
# The examplePath parameters specifies the folder where the example files are located
macro(add_doxygen_doc packageName sourceCodePath examplesPath)

	if (ENABLE_DOC)

		find_package(Doxygen)
		
		if (DOXYGEN_FOUND STREQUAL "NO")
		    message("Doxygen not found. Please get a copy http://www.doxygen.org")
		else()
			
			# prepare doxygen configuration file
			set(DOXYGEN_SRCDIR ${sourceCodePath})
			set(OUTDIR ${CMAKE_CURRENT_BINARY_DIR})
			set(DOXYGEN_EXAMPLES_PATH ${examplesPath})
			set(DOXYGEN_PROJECT_NAME ${packageName})
			configure_file(doxygen.cfg.in ${CMAKE_CURRENT_BINARY_DIR}/doxygen.cfg)
			
			# add doxygen as target
			add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doxygen COMMAND ${DOXYGEN_EXECUTABLE} ARGS ${CMAKE_CURRENT_BINARY_DIR}/doxygen.cfg)
			
			# cleanup $build/doc/doxygen on "make clean"
			set_property(DIRECTORY APPEND PROPERTY
				     ADDITIONAL_MAKE_CLEAN_FILES doxygen)
			
			add_custom_target(
				doc ALL
				DEPENDS
				${CMAKE_CURRENT_BINARY_DIR}/doxygen
			)
			
			# install HTML API documentation and manual pages
			set(DOC_PATH "share/doc/${packageName}")
			
			install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doxygen
			         DESTINATION ${DOC_PATH}
			)
		
		endif()

	endif()

endmacro()


add_doxygen_doc(someip "${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../README.md" "${CMAKE_CURRENT_SOURCE_DIR}/../examples ${CMAKE_CURRENT_SOURCE_DIR}/..")
