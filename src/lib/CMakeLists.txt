add_subdirectory(network)
add_subdirectory(backend)
add_subdirectory(ortc)
add_subdirectory(messenger)
add_subdirectory(session)
add_subdirectory(storage)
add_subdirectory(audio)
add_subdirectory(video)
add_subdirectory(board)
add_subdirectory(ui)

if(ENABLE_PLUGINS)
  add_subdirectory(plugin)
endif()
