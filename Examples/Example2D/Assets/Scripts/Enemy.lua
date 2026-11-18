-- chimera-uuid: 4e9d5300-2351-4a05-bc32-b31c6d6e8a97
-- An enemy saucer: weaves down the screen, fires its pattern through the game's enemy
-- bullets, flashes when hit and explodes and dissolves when its health runs out. The flash
-- and the dissolve are sprite materials (Game.materials); the dissolve reads how much is
-- left from the alpha of the sprite's color, so each enemy dissolves at its own pace.
---@class Enemy : Script
---@field health integer
---@field speed number
---@field sway number
---@field fireInterval number
---@field points integer
---@field pattern string
---@field bottom number
local Enemy = {}

Enemy.properties = {
  { "health", 3, type = "integer", min = 1, max = 50 },
  { "speed", 1.6, tooltip = "Units per second down the screen" },
  { "sway", 2.5, tooltip = "How far it weaves to each side" },
  { "fireInterval", 1.6, min = 0.2, max = 10, tooltip = "Seconds between shots" },
  { "points", 100, type = "integer" },
  { "pattern", "ring", tooltip = "ring, spread (at the player) or aim (homing)" },
  { "bottom", -9.5, tooltip = "Below this height it leaves" },
}

-- Seconds the dissolve takes after the enemy dies.
local DISSOLVE_TIME = 0.4

function Enemy:onStart()
  self.hitTarget = self.gameObject:addProjectileTarget(0.5, 2)
  self.sprite = self.gameObject:getSprite()
  self.position = Vector3()
  self.target = Vector3()
  self.age = 0
  self.flash = 0
  self.dying = -1
  self.cooldown = 0.6 + math.random() * self.fireInterval
  self.startY = self.transform:getWorldPosition(self.position).y
  self.phase = math.random() * 6.28
end

function Enemy:update(dt)
  self.age = self.age + dt
  local z = self.transform:getWorldPosition(self.position).z - self.speed * dt
  local y = self.startY + math.sin(self.age * 1.3 + self.phase) * self.sway
  self.transform:setWorldPosition(0, y, z)
  if z < self.bottom then
    self.gameObject:destroy()
    return
  end

  if self.dying >= 0 then
    self.dying = self.dying + dt
    local left = 1 - self.dying / DISSOLVE_TIME
    if left <= 0 then
      self.gameObject:destroy()
    else
      self.sprite:setColor(Color(1, 1, 1, left))
    end
    return
  end

  if self.flash > 0 then
    self.flash = self.flash - dt
    if self.flash <= 0 then
      self:setFlash(false)
    end
  end

  if not Game or Game.over then
    return
  end
  self.cooldown = self.cooldown - dt
  if self.cooldown <= 0 then
    self:fire(y, z)
    self.cooldown = self.fireInterval
  end
end

function Enemy:fire(y, z)
  local bullets = Game.enemyBullets
  Game:playSound(Game.sounds.enemyShot, 0.3)
  if self.pattern == "ring" then
    bullets:fireRing(y, z, 12, self.age * 40, 3.5)
    return
  end

  -- Angles are degrees counterclockwise from +Y (screen right).
  local target = Game.player:getTransform():getWorldPosition(self.target)
  local angle = math.deg(math.atan(target.z - z, target.y - y))
  if self.pattern == "spread" then
    bullets:fireSpread(y, z, 5, angle, 50, 4.5)
  else
    bullets:spawn{ x = y, y = z, angle = angle, speed = 3, turnRate = 70, lifetime = 5 }
  end
end

function Enemy:damage(amount)
  if self.health <= 0 then
    return
  end
  self.health = self.health - amount
  self.flash = 0.08
  self:setFlash(true)
  if self.health <= 0 then
    local position = self.transform:getWorldPosition(self.position)
    Game:enemyKilled(position.y, position.z, self.points)
    -- On a team no bullet hits, so the dissolving saucer is no longer a target.
    self.hitTarget:setTeam(32)
    self.dying = 0
    self.sprite:setMaterial(Game.materials.dissolve)
  end
end

-- The flash material swapped in and out; without one (an old project) the color brightens.
function Enemy:setFlash(on)
  local material = Game and Game.materials.hitFlash
  if material then
    self.sprite:setMaterial(on and material or nil)
  else
    -- Colors above one brighten the sprite.
    self.sprite:setColor(on and Color(3, 3, 3, 1) or Color(1, 1, 1, 1))
  end
end

return Enemy
