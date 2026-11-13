-- chimera-uuid: 18ae49fd-5b21-4824-bd14-0b07e2899437
-- Runs the shooter: spawns enemies, owns the enemy bullets and the explosion sparks, keeps
-- the score and opens the map again after a game over. Other scripts reach it through the
-- global Game, set when it starts.
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
end

function GameManager:update(dt)
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
  if self.kills % 10 == 0 then
    Log.info("Score " .. self.score)
  end
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
