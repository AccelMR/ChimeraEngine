-- chimera-uuid: 18ae49fd-5b21-4824-bd14-0b07e2899437
-- Runs the shooter: spawns enemies, owns the enemy bullets, the explosion sparks and the
-- sounds, keeps the score (drawn in the top right corner) and opens the map again after a
-- game over. Other scripts reach it through the global Game, set when it starts.
---@class GameManager : Script
---@field enemy Prefab
---@field spawnInterval number
---@field minSpawnInterval number
---@field halfWidth number
---@field halfHeight number
---@field stressNumbers integer
---@field stressParticles integer
---@field stressLifetime number
local GameManager = {}

-- Damage numbers by size: plain, gold, orange and flashing, huge red and shaking.
local DAMAGE_TIERS = {
  { min = 0, color = Color(1, 1, 1, 1), outline = 2, outlineColor = Color(0, 0, 0, 1),
    pop = 1.3, popDuration = 0.12 },
  { min = 1000, color = Color(1, 0.8, 0.15, 1), outline = 3,
    outlineColor = Color(0.35, 0.12, 0, 1), scale = 1.3, pop = 1.8, popDuration = 0.15 },
  { min = 10000, color = Color(1, 0.4, 0.08, 1), outline = 3,
    outlineColor = Color(0.3, 0, 0, 1), scale = 1.7, pop = 2.4, popDuration = 0.18,
    flash = 0.25, flashColor = Color(1, 1, 0.6, 1), flashRate = 12 },
  { min = 100000, color = Color(1, 0.12, 0.18, 1), outline = 4,
    outlineColor = Color(0.15, 0, 0.05, 1), scale = 2.3, pop = 3, popDuration = 0.2,
    shake = 0.12, shakeFrequency = 25, shakeDuration = 0.35, flash = 0.4,
    flashColor = Color(1, 1, 1, 1), flashRate = 14 },
}

-- Particles of the stress test leave from a point in bursts of this many.
local STRESS_RING = 64

GameManager.properties = {
  { "enemy", type = "Prefab", tooltip = "What spawns at the top of the screen" },
  { "spawnInterval", 1.4, min = 0.2, max = 5, tooltip = "Seconds between enemies at the start" },
  { "minSpawnInterval", 0.35, min = 0.1, max = 2, tooltip = "The fastest spawning gets" },
  { "halfWidth", 13.5, tooltip = "Half the width the camera shows, in world units" },
  { "halfHeight", 8, tooltip = "Half the height the camera shows, in world units" },
  { "stressNumbers", 75000, type = "integer", min = 0, max = 200000,
    tooltip = "Damage numbers the stress test (P) keeps alive; changes apply while it runs" },
  { "stressParticles", 75000, type = "integer", min = 0, max = 200000,
    tooltip = "Particles the stress test (P) keeps alive" },
  { "stressLifetime", 1.2, min = 0.3, max = 5,
    tooltip = "Seconds a stress number lives; particles live a little longer" },
}

function GameManager:onStart()
  Game = self
  self.score = 0
  self.kills = 0
  self.timer = 1.0
  self.over = false
  self.player = Scene.find("Player")

  -- Clips are found once; playing one is a C call with plain numbers, so a sound per shot
  -- makes no garbage. Each clip's voice limit (set in the Asset Inspector) keeps rapid fire
  -- from taking every voice.
  self.sounds = { shot = AudioClip.find("S_ShotPlayer"), enemyShot = AudioClip.find("S_ShotEnemy"),
                  explosion = AudioClip.find("S_Explosion"), hurt = AudioClip.find("S_Hurt") }
  -- Sprite materials the enemies swap to: a white flash when hit, a dissolve when they die.
  -- Each is one shared asset; what differs per enemy rides in its sprite's color.
  self.materials = { hitFlash = SpriteMaterial.find("SM_HitFlash"),
                     dissolve = SpriteMaterial.find("SM_Dissolve") }
  -- The camera's Post Process holds the engine's CRT effect; C turns it on and off.
  self.postProcess = Scene.find("Camera"):getPostProcess()
  -- The music is a global voice: opening the map again after a game over runs onStart again,
  -- and the music keeps playing through it instead of starting a second time.
  if not (MusicVoice and MusicVoice:isPlaying()) then
    MusicVoice = Audio.play("S_Music", { loop = true, volume = 0.45, fadeIn = 1.5 })
  end

  -- Every enemy shoots through this one system, which hits team 1: the player.
  self.enemyBullets = self.gameObject:addProjectiles()
  self.enemyBullets:configure{ speed = 5, lifetime = 7, radius = 0.12, damage = 1,
                               capacity = 4096, cellSize = 1 }
  self.enemyBullets:setLook(Sprites.makeStyle{ texture = "T_BulletEnemy", pixelsPerUnit = 32,
                                               blend = "Additive", layer = 3 })
  self.enemyBullets:setHitTeams({ 1 })
  -- Bullets spawned with a turn rate steer toward the player.
  self.enemyBullets:setHomingTarget(self.player)
  self.enemyBullets:onHit(function(target, damage)
    local ship = target:getScript("PlayerShip")
    if ship then
      ship:damage(damage)
    end
  end)

  -- Explosions are particles: each spark is written once and the GPU moves it, cools its
  -- color from white to red and shrinks it as it ages.
  self.sparks = Scene.find("Sparks"):addParticles()
  self.sparks:configure{ capacity = 4096, rate = 0, playOnStart = false, spread = 180,
                         speed = 4.5, speedVariance = 1.5, drag = 1.5,
                         gravity = Vector2(0, -6), lifetime = 0.5, lifetimeVariance = 0.15,
                         fadeIn = 0, fadeOut = 0.15, minScale = 0.7, maxScale = 1.3 }
  self.sparks:setLook(Sprites.makeStyle{ texture = "T_Spark", pixelsPerUnit = 32,
                                         blend = "Additive", layer = 4 })
  self.sparks:setColorGradient{ { 0, Color(1, 1, 0.85, 1) }, { 0.4, Color(1, 0.65, 0.2, 1) },
                                { 1, Color(1, 0.2, 0.1, 1) } }
  self.sparks:setSizeCurve{ { 0, 1.2 }, { 1, 0.3 } }

  -- Styles are made once; drawing one each frame is a single C call. The score is given as a
  -- number, so it is written in C without making a Lua string every frame.
  self.scoreStyle = Text.makeStyle{ size = 0.8, pivot = Vector2(1, 1), monospaceDigits = true,
                                    layer = 10, outlineWidth = 3, notation = "Suffix",
                                    plainBelow = 1000000 }
  self.labelStyle = Text.makeStyle{ size = 0.4, pivot = Vector2(1, 1), layer = 10,
                                    color = Color(0.7, 0.8, 1, 1) }
  self.titleStyle = Text.makeStyle{ size = 2, pivot = Vector2(0.5, 0.5), layer = 10,
                                    color = Color(1, 0.35, 0.3, 1), outlineWidth = 4,
                                    shadowColor = Color(0, 0, 0, 0.8),
                                    shadowOffset = Vector2(0.1, -0.1) }
  self.hintStyle = Text.makeStyle{ size = 0.6, pivot = Vector2(0.5, 0.5), layer = 10,
                                   outlineWidth = 2 }

  -- Damage numbers: each one is written once and the GPU flies it along an arc, pops,
  -- shakes, flashes and fades it, so thousands cost nothing per frame. Bigger numbers reach
  -- higher tiers: another color, bigger, and more animation.
  self.damageNumbers = self.gameObject:addFloatingNumbers()
  self.damageNumbers:configure{ size = 0.45, capacity = 20000, angle = 90, spread = 35,
                                speed = 6, speedVariance = 2, gravity = Vector2(0, -14),
                                lifetime = 0.9, lifetimeVariance = 0.2, fadeOut = 0.35,
                                layer = 11, notation = "Suffix", decimals = 1 }
  self.damageNumbers:setTiers(DAMAGE_TIERS)

  -- The stress test lives on the Background object, which has no projectiles or numbers of
  -- its own; it is made the first time P is pressed.
  self.stressOn = false
  self.frameTime = 1 / 60
  -- The worst frame of the last two seconds, and when the stress test last turned on, so a
  -- hitch can be told apart from a slow average.
  self.worstFrame = 0
  self.worstFrameAge = 0
  self.stressStarted = -1
  self.stressStyle = Text.makeStyle{ size = 0.4, pivot = Vector2(0, 1), layer = 12,
                                     outlineWidth = 3, color = Color(0.6, 1, 0.6, 1) }
  self.stressHintStyle = Text.makeStyle{ size = 0.3, pivot = Vector2(0, 0), layer = 12,
                                         color = Color(0.6, 0.7, 0.9, 1) }
end

-- P turns it on and off: stressNumbers numbers and stressParticles particles stay alive,
-- spawned at the rate that keeps the count as they die. The three properties are sliders in
-- the Inspector, so the load can be tuned while it runs.
function GameManager:applyStress()
  local numbers = self.stressNumbers
  local particles = self.stressParticles
  local lifetime = self.stressLifetime
  self.appliedStress = { numbers, particles, lifetime }
  if not self.stressNumberSystem then
    local host = Scene.find("Background")
    self.stressParticleSystem = host:addParticles()
    self.stressParticleSystem:setLook(Sprites.makeStyle{ texture = "T_Spark", pixelsPerUnit = 48,
                                                          blend = "Additive", layer = 1 })
    self.stressNumberSystem = host:addFloatingNumbers()
    self.stressNumberSystem:setTiers(DAMAGE_TIERS)
  end
  local particleLifetime = lifetime * 1.2
  self.stressParticleSystem:configure{ capacity = math.max(particles, 1), rate = 0,
                                       playOnStart = false, spread = 180, speed = 3,
                                       speedVariance = 2, gravity = Vector2(0, -1),
                                       lifetime = particleLifetime, lifetimeVariance = 0,
                                       fadeIn = 0, fadeOut = 0 }
  -- An outlined number takes two runs.
  self.stressNumberSystem:configure{ capacity = numbers * 2 + 4096, size = 0.25, angle = 90,
                                     spread = 60, speed = 3, speedVariance = 1.5,
                                     gravity = Vector2(0, -5), lifetime = lifetime,
                                     lifetimeVariance = 0, fadeOut = math.min(0.3, lifetime * 0.5),
                                     layer = 11, notation = "Suffix", decimals = 1 }
  self.stressNumberRate = numbers / lifetime
  self.stressParticleRate = particles / particleLifetime
  self.stressNumberDebt = 0
  self.stressParticleDebt = 0
end

function GameManager:stopStress()
  if self.stressNumberSystem then
    self.stressNumberSystem:clear()
    self.stressParticleSystem:clear()
  end
end

-- Mostly small numbers, so the screen is not all huge criticals: 70% below a thousand, 2%
-- in the top tier.
local function stressValue(random)
  local roll = random()
  local power = roll < 0.7 and 0 or roll < 0.9 and 1 or roll < 0.98 and 2 or 3
  return random(10, 999) * 10 ^ power
end

function GameManager:updateStress(dt)
  if Input.wasActionPressed("Stress") then
    self.stressOn = not self.stressOn
    if self.stressOn then
      self.stressStarted = Time.time
      self:applyStress()
    else
      self:stopStress()
    end
  end
  self.frameTime = self.frameTime * 0.95 + dt * 0.05
  self.worstFrameAge = self.worstFrameAge + dt
  if dt > self.worstFrame or self.worstFrameAge > 2 then
    self.worstFrame = dt
    self.worstFrameAge = 0
  end
  if dt > 0.05 then
    Log.warning(string.format("Long frame: %.1f ms, %.1f s after the start, %s", dt * 1000,
                              Time.time, self.stressStarted >= 0
                                and string.format("%.2f s after the stress test started",
                                                  Time.time - self.stressStarted)
                                or "before the stress test"))
  end
  if not self.stressOn then
    Text.draw(self.stressHintStyle, "P: stress test    C: CRT", -self.halfWidth + 0.3,
              -self.halfHeight + 0.2)
    return
  end
  local applied = self.appliedStress
  if applied[1] ~= self.stressNumbers or applied[2] ~= self.stressParticles or
     applied[3] ~= self.stressLifetime then
    self:applyStress()
  end

  -- A long frame must not spawn a burst that makes the next one longer.
  dt = math.min(dt, 0.1)
  local random = math.random
  local w, h = self.halfWidth, self.halfHeight
  self.stressNumberDebt = self.stressNumberDebt + self.stressNumberRate * dt
  local count = math.floor(self.stressNumberDebt)
  self.stressNumberDebt = self.stressNumberDebt - count
  local numbers = self.stressNumberSystem
  for _ = 1, count do
    numbers:spawn(stressValue(random), (random() * 2 - 1) * w, (random() * 2 - 1) * h)
  end

  self.stressParticleDebt = self.stressParticleDebt + self.stressParticleRate * dt
  local rings = math.floor(self.stressParticleDebt / STRESS_RING)
  self.stressParticleDebt = self.stressParticleDebt - rings * STRESS_RING
  local particles = self.stressParticleSystem
  for _ = 1, rings do
    particles:emit(STRESS_RING, (random() * 2 - 1) * w, (random() * 2 - 1) * h)
  end

  Text.draw(self.stressStyle,
            string.format("STRESS %.0fK numbers + %.0fK particles (P)\nNumber runs: %d\n"
                          .. "Particles: %d\nFrame: %.2f ms (%d FPS)\nWorst frame: %.1f ms",
                          self.stressNumbers / 1000, self.stressParticles / 1000,
                          numbers:getCount(), particles:getCount(), self.frameTime * 1000,
                          math.floor(1 / self.frameTime + 0.5), self.worstFrame * 1000),
            -self.halfWidth + 0.3, self.halfHeight - 0.9)
end

-- The damage is made up: most hits are small, some are criticals and a few are huge, so
-- every tier shows.
function GameManager:showDamage(x, y)
  local damage = math.random(40, 900)
  local roll = math.random()
  if roll < 0.03 then
    damage = damage * math.random(150, 400)
  elseif roll < 0.15 then
    damage = damage * math.random(12, 40)
  elseif roll < 0.4 then
    damage = damage * math.random(2, 5)
  end
  self.damageNumbers:spawn(damage, x, y + 0.3)
end

function GameManager:drawHud()
  local right = self.halfWidth - 0.3
  local top = self.halfHeight - 0.2
  Text.draw(self.labelStyle, "SCORE", right, top)
  Text.draw(self.scoreStyle, self.score, right, top - 0.45)
  if self.over then
    Text.draw(self.titleStyle, "GAME OVER", 0, 1)
    Text.draw(self.hintStyle, "Press R (or Start) to play again", 0, -0.6)
  end
end

function GameManager:update(dt)
  self:drawHud()
  self:updateStress(dt)
  if Input.wasActionPressed("ToggleCRT") and self.postProcess
     and self.postProcess:getPassCount() > 0 then
    self.postProcess:setEnabled(1, not self.postProcess:isEnabled(1))
  end
  if self.over then
    if Input.wasActionPressed("Restart") then
      Scene.openMap("Main")
    end
    return
  end

  self.timer = self.timer - dt
  if self.timer <= 0 and self.enemy then
    self:spawnEnemy()
    -- Faster as enemies fall.
    self.timer = math.max(self.minSpawnInterval, self.spawnInterval - self.kills * 0.02)
  end
end

function GameManager:spawnEnemy()
  local y = (math.random() * 2 - 1) * (self.halfWidth - 1.5)
  local enemy = Scene.instantiate(self.enemy, nil, Vector3(0, y, self.halfHeight + 1))
  local script = enemy:getScript("Enemy")
  if script then
    local patterns = { "ring", "spread", "aim" }
    script.pattern = patterns[math.random(#patterns)]
  end
end

function GameManager:explode(y, z, count)
  self.sparks:emit(count, y, z)
  -- Bigger explosions sound lower.
  self:playSound(self.sounds.explosion, 0.6, 1.25 - math.min(count, 48) / 80)
end

-- Plays a sound with a small random change of pitch, so repeated sounds do not all match.
function GameManager:playSound(clip, volume, pitch)
  if clip then
    Audio.play(clip, volume, (pitch or 1) * (0.94 + math.random() * 0.12))
  end
end

function GameManager:enemyKilled(y, z, points)
  self.score = self.score + points
  self.kills = self.kills + 1
  self:explode(y, z, 20)
end

function GameManager:gameOver()
  if self.over then
    return
  end
  self.over = true
  self.enemyBullets:clear()
  Log.info("Game over. Score " .. self.score .. ". Press R (or Start) to play again.")
end

return GameManager
