# This file is part of Desktop App Toolkit,
# a set of libraries for developing nice desktop applications.
#
# For license and copyright information please follow this link:
# https://github.com/desktop-app/legal/blob/master/LEGAL

set(MAXIMUM_CXX_STANDARD cxx_std_20)

function(init_target_folder target_name folder_name)
	if (NOT "${folder_name}" STREQUAL "")
		set_target_properties(${target_name} PROPERTIES FOLDER ${folder_name})
	endif ()
endfunction()

function(init_target target_name) # init_target(my_target folder_name)
	if (ARGC GREATER 1)
		if (${ARGV1} STREQUAL cxx_std_14 OR ${ARGV1} STREQUAL cxx_std_11 OR ${ARGV1} STREQUAL cxx_std_17)
			target_compile_features(${target_name} PRIVATE ${ARGV1})
		else ()
			target_compile_features(${target_name} PRIVATE ${MAXIMUM_CXX_STANDARD})
			init_target_folder(${target_name} ${ARGV1})
		endif ()
	else ()
		target_compile_features(${target_name} PRIVATE ${MAXIMUM_CXX_STANDARD})
	endif ()
	if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		set_target_properties(${target_name} PROPERTIES
				MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif ()
	target_link_libraries(${target_name} PRIVATE desktop-app::common_options)
	set_target_properties(${target_name} PROPERTIES
			XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK YES
			XCODE_ATTRIBUTE_GCC_INLINES_ARE_PRIVATE_EXTERN YES
			XCODE_ATTRIBUTE_GCC_SYMBOLS_PRIVATE_EXTERN YES
	)
	if (DESKTOP_APP_SPECIAL_TARGET)
		set_target_properties(${target_name} PROPERTIES
				XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL $<IF:$<CONFIG:Debug>,0,fast>
				XCODE_ATTRIBUTE_LLVM_LTO $<IF:$<CONFIG:Debug>,NO,YES>
		)
	endif ()
	if (DESKTOP_APP_SPECIAL_TARGET AND WIN32 AND NOT build_win64)
		set_target_properties(${target_name} PROPERTIES
				INTERPROCEDURAL_OPTIMIZATION_RELEASE True
				INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO True
				INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL True
		)
	endif ()
endfunction()

# This code is not supposed to run on build machine, only on target machine.
function(init_non_host_target target_name)
	init_target(${target_name})
	if (APPLE)
		set_target_properties(${target_name} PROPERTIES
				OSX_ARCHITECTURES "${DESKTOP_APP_MAC_ARCH}"
		)
	endif ()
endfunction()


# 定义一个自定义目标，在构建前执行
add_custom_target(PreBuild
	  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMAND ${CMAKE_COMMAND} -E echo "Running pre-build step..."
		# COMMAND bash ${CMAKE_SOURCE_DIR}/prebuild.sh # Linux/Mac 示例
		# COMMAND ${CMAKE_SOURCE_DIR}/prebuild.bat # Windows 示例
		#	if error occurred please execute: `sudo pip3 install openpyxl [--break-system-packages]`
		COMMAND python3 translation.py
		COMMENT "Executing pre-build step"
)

