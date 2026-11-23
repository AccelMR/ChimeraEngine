ch_add_module(chUtilities EXPORT_DEFINE CH_UTILITY_EXPORTS FOLDER Engine)

# PUBLIC, so every module sees the same profiler and console macros chUtilities was built
# with.
target_compile_definitions(chUtilities PUBLIC
  $<$<BOOL:${CH_ENABLE_PROFILER}>:CH_PROFILER_ENABLED>
  $<$<BOOL:${CH_ENABLE_CONSOLE}>:CH_CONSOLE_ENABLED>)

if(WIN32)
  # <windows.h> defines min and max as macros, which break std::min, std::max and
  # numeric_limits<T>::max(). PUBLIC, so it covers every module and include order.
  target_compile_definitions(chUtilities PUBLIC NOMINMAX)
endif()

# Each module keeps its tests in its Test folder.
if(CH_BUILD_TESTS)
  add_subdirectory(Test)
endif()
