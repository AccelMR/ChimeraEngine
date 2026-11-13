-- chimera-uuid: 4e9d5300-2351-4a05-bc32-b31c6d6e8a97
-- An enemy saucer: weaves down the screen, fires its pattern through the game's enemy
-- bullets, flashes when hit and explodes when its health runs out.
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

function Enemy:onStart()
  self.gameObject:addProjectileTarget(0.5, 2)
  self.sprite = self.gameObject:getSprite()
  self.position = Vector3()
  self.target = Vector3()
  self.age = 0
  self.flash = 0
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

  if self.flash > 0 then
    self.flash = self.flash - dt
    if self.flash <= 0 then
      self.sprite:setColor(Color(1, 1, 1, 1))
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
  -- Colors above one brighten the sprite.
  self.flash = 0.08
  self.sprite:setColor(Color(3, 3, 3, 1))
  if self.health <= 0 then
    local position = self.transform:getWorldPosition(self.position)
    Game:enemyKilled(position.y, position.z, self.points)
    self.gameObject:destroy()
  end
end

return Enemy
