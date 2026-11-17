# Example2D

A small 2D shooter made with Chimera's 2D tools. Move with WASD, the arrows or the left
stick; fire with Space, the left mouse button or the South button; after a game over, R or
Start plays again; P (or Back) turns a stress test of numbers and particles on and off.

Open it with `chEditor -project=Examples/Example2D` (the editor starts in its 2D view) or
play it with `chGame -project=Examples/Example2D`.

## What it shows

| Piece | Where |
|---|---|
| The Sprite2D render path | `Config/Engine.ini` (`[Renderer] Path=Sprite2D`) |
| Engine plugins enabled by the project | `Example2D.chproject` (`[Plugins] Enabled=chProjectiles,chParticles`) |
| An orthographic camera looking along +X | `Camera` object in `Maps/Main` |
| Sprite components and an animated atlas | `Player`, the `Enemy` prefab, `Sprites/SA_Enemy` (animation "Fly") |
| Thousands of bullets without GameObjects | `GameManager.lua` and `PlayerShip.lua` (`addProjectiles`, `configure`, `fireRing`, `fireSpread`, homing with `spawn{turnRate}`) |
| Explosions and the stress particles as GPU particles: written once, colored along a gradient and shrunk along a curve as they age | `explode` and `applyStress` in `GameManager.lua` (the chParticles plugin: `addParticles`, `configure`, `setLook`, `setColorGradient`, `setSizeCurve`, `emit`) |
| Hits reported to scripts | `onHit` in `PlayerShip.lua` and `GameManager.lua`, targets added with `addProjectileTarget` |
| Sprites drawn every frame without GameObjects | `Background.lua` (star field), the lives in `PlayerShip.lua` (`Sprites.draw`) |
| Text and numbers drawn every frame | the score and the game over screen in `GameManager.lua` (`Text.makeStyle`, `Text.draw` with a number) |
| A stress test: P turns on 75K damage numbers and 75K particles (the `stressNumbers`, `stressParticles` and `stressLifetime` properties of `Game`, sliders in the Inspector that apply while it runs), with the frame time on screen | `updateStress` in `GameManager.lua` |
| Damage numbers that fly, pop, shake and flash on the GPU | `showDamage` in `GameManager.lua`, called by the bullets' `onHit` in `PlayerShip.lua` (the chParticles plugin: `addFloatingNumbers`, `configure`, `setTiers`, `spawn`) |
| Spawning from a prefab property | `GameManager.lua` (`Scene.instantiate`, the `enemy` property) |
| Opening a map again | `GameManager.lua` (`Scene.openMap("Main")`) |
| Input actions | `Config/Input.ini` (Move, Fire, Restart) |
| Sounds and music | `GameManager.lua` (`AudioClip.find`, `Audio.play` with plain numbers, the looping streamed music kept in the global `MusicVoice` across map reloads), called from `PlayerShip.lua` and `Enemy.lua`; each clip's voice limit is set in the Asset Inspector |

The world is the YZ plane: Y is right and Z is up, and in scripts the x and y of 2D
functions are world Y and Z. Angles are degrees counterclockwise from +Y, so 90 is up.
Teams: the player is team 1, enemies team 2; each projectile system says which teams it
hits.

## Sounds

`SourceAssets/Sounds` holds `generate_sounds.py`, which makes the WAV files with plain
Python (`python generate_sounds.py`): two shots, an explosion, a hurt sound and a 17 second
music loop at 22050 Hz. The WAV files are not kept, since an asset holds its whole file and
the script makes them again to import them anew. They were imported into `Assets/Audio` as
`S_*`; the music is longer than `Audio.ImportStreamSeconds`, so it streams on the music bus,
and the shots and explosions have a voice limit (6 and 8) so rapid fire keeps voices for the
rest.

## Images

`SourceAssets/Images` holds the PNG files and `generate_images.py`, which draws them with
plain Python (`python generate_images.py`). They were imported into `Assets/Textures`
without mips; the enemy sheet became the atlas `SA_Enemy` (4 frames in a row).
