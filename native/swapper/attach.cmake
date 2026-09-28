# Compile in the host's own CRT/Qt mode rather than mixing static MSVC runtimes.
function(shiny_attach_swapper target)
  if(WIN32)
    target_sources(${target} PRIVATE
      "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/service.cpp"
      "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/dialog.cpp")
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(${target} PRIVATE bcrypt comctl32 comdlg32 shell32 ole32 dwmapi)
  endif()
endfunction()
