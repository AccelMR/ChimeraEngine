# Engine plugins: features a project enables in its .chproject ([Plugins] Enabled=) instead of
# every game carrying them. Each one adds its target name to CH_ENGINE_PLUGINS, so the editor
# and chGame build it with them.
set(CH_ENGINE_PLUGINS "" PARENT_SCOPE)

add_subdirectory(chProjectiles)
add_subdirectory(chParticles)

set(CH_ENGINE_PLUGINS chProjectiles chParticles PARENT_SCOPE)
