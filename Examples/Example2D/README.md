# Example2D

A small 2D shooter made with Chimera's 2D tools. Move with WASD, the arrows or the left
stick; fire with Space, the left mouse button or the South button; after a game over, R or
Start plays again.

Open it with `chEditor -project=Examples/Example2D` (the editor starts in its 2D view) or
play it with `chGame -project=Examples/Example2D`.

## What it shows

| Piece | Where |
|---|---|
| The Sprite2D render path | `Config/Engine.ini` (`[Renderer] Path=Sprite2D`) |
| An engine plugin enabled by the project | `Example2D.chproject` (`[Plugins] Enabled=chProjectiles`) |
| An orthographic camera looking along +X | `Camera` object in `Maps/Main` |
| Sprite components and an animated atlas | `Player`, the `Enemy` prefab, `Sprites/SA_Enemy` (animation "Fly") |
| Thousands of bullets without GameObjects | `GameManager.lua` and `PlayerShip.lua` (`addProjectiles`, `configure`, `fireRing`, `fireSpread`, homing with `spawn{turnRate}`) |
| Hits reported to scripts | `onHit` in `PlayerShip.lua` and `GameManager.lua`, targets added with `addProjectileTarget` |
| Sprites drawn every frame without GameObjects | `Background.lua` (star field), the lives in `PlayerShip.lua` (`Sprites.draw`) |
| Text and numbers drawn every frame | the score and the game over screen in `GameManager.lua` (`Text.makeStyle`, `Text.draw` with a number) |
| Spawning from a prefab property | `GameManager.lua` (`Scene.instantiate`, the `enemy` property) |
| Opening a map again | `GameManager.lua` (`Scene.openMap("Main")`) |
| Input actions | `Config/Input.ini` (Move, Fire, Restart) |

The world is the YZ plane: Y is right and Z is up, and in scripts the x and y of 2D
functions are world Y and Z. Angles are degrees counterclockwise from +Y, so 90 is up.
Teams: the player is team 1, enemies team 2; each projectile system says which teams it
hits.

## Images

`SourceAssets/Images` holds the PNG files and `generate_images.py`, which draws them with
plain Python (`python generate_images.py`). They were imported into `Assets/Textures`
without mips; the enemy sheet became the atlas `SA_Enemy` (4 frames in a row).
