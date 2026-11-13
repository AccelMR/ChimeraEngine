-- chimera-uuid: 18ae49fd-5b21-4824-bd14-0b07e2899437
-- Runs the shooter: spawns enemies, owns the enemy bullets and the explosion sparks, keeps
-- the score (drawn in the top right corner) and opens the map again after a game over. Other
-- scripts reach it through the global Game, set when it starts.
---@class GameManager : Script
---@field enemy Prefab
---@field spawnInterval number
---@field minSpawnInterval number
---@field halfWidth number
---@field halfHeight number
local GameManager = {}

GameManager.properties = {
  { "enemy", type = "Prefab", tooltip = "What spawns at the top of the screen" },
  { "spawnInterval", 1.4, min = 0.2, max = 5, tooltip = "Seconds between enemies at the start" },
  { "minSpawnInterval", 0.35, min = 0.1, max = 2, tooltip = "The fastest spawning gets" },
  { "halfWidth", 13.5, tooltip = "Half the width the camera shows, in world units" },
  { "halfHeight", 8, tooltip = "Half the height the camera shows, in world units" },
}

function GameManager:onStart()
  Game = self
  self.score = 0
  self.kills = 0
  self.timer = 1.0
  self.over = false
  self.player = Scene.find("Player")

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

  -- Explosions are projectiles that hit nothing, fall a little and fade with their lifetime.
  self.sparks = Scene.find("Sparks"):addProjectiles()
  self.sparks:configure{ speed = 4, lifetime = 0.45, radius = 0, capacity = 2048,
                         acceleration = Vector2(0, -6) }
  self.sparks:setLook(Sprites.makeStyle{ texture = "T_Spark", pixelsPerUnit = 32,
                                         blend = "Additive", layer = 4 }, false)
  self.sparks:setHitTeams({})

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
  self.damageNumbers:setTiers{
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
  self.sparks:fireRing(y, z, count, math.random() * 360)
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
