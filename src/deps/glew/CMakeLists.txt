cmake_minimum_required(VERSION 2.6)
SET(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "add a postfix, usually d on windows")
SET(CMAKE_RELEASE_POSTFIX "" CACHE STRING "add a postfix, usually empty on windows")
project(glew)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

if( NOT WIN32 AND NOT APPLE )
  find_package(X11)
  include_directories(${X11_X11_INCLUDE_PATH})
endif()

if( MSVC )
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
	
	if (NOT SGCT_USE_MSVC_RUNTIMES)
        foreach (flag ${SGCTCompilerFlags})

            if (${flag} MATCHES "/MD")
                string(REGEX REPLACE "/MD" "/MT" ${flag} "${${flag}}")
            endif()
            if (${flag} MATCHES "/MDd")
                string(REGEX REPLACE "/MDd" "/MTd" ${flag} "${${flag}}")
            endif()

        endforeach()
    endif()
	
	foreach(CompilerFlag ${SGCTCompilerFlags})
		string(REPLACE "/Zi" "/Z7" ${CompilerFlag} "${${CompilerFlag}}")
	endforeach()
endif()

if(MSVC AND NOT "${MSVC_VERSION}" LESS 1400)
	add_definitions( "/MP" )
endif()

# C Pre-processor definitions for all targets
add_definitions(-DGLEW_BUILD)
add_definitions(-DGLEW_NO_GLU)
add_definitions(-DGLEW_STATIC)

add_library(glew STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/glew.c)
set_target_properties(glew PROPERTIES FOLDER "Dependencies")