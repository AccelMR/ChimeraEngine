-- chimera-uuid: 1ee9b6d0-86ce-4d01-b9a0-94fee55c9ba2
-- The player's ship: moves with Move, fires with Fire, is hit by the enemy bullets, and
-- draws its remaining lives in the top left corner.
---@class PlayerShip : Script
---@field speed number
---@field fireInterval number
---@field maxHealth integer
---@field halfWidth number
---@field halfHeight number
local PlayerShip = {}

PlayerShip.properties = {
  { "speed", 9, min = 1, max = 30, tooltip = "Units per second" },
  { "fireInterval", 0.12, min = 0.03, max = 1, tooltip = "Seconds between shots" },
  { "maxHealth", 5, type = "integer", min = 1, max = 20 },
  { "halfWidth", 13, tooltip = "How far it can go to each side" },
  { "halfHeight", 7.5, tooltip = "How far it can go up and down" },
}

function PlayerShip:onStart()
  self.health = self.maxHealth
  self.cooldown = 0
  self.flash = 0
  -- Kept and filled every frame, so moving allocates nothing.
  self.move = Vector2()
  self.position = Vector3()
  self.sprite = self.gameObject:getSprite()
  self.gameObject:addProjectileTarget(0.35, 1)

  self.bullets = self.gameObject:addProjectiles()
  self.bullets:configure{ speed = 20, lifetime = 1.2, radius = 0.2, damage = 1, capacity = 512 }
  self.bullets:setLook(Sprites.makeStyle{ texture = "T_BulletPlayer", pixelsPerUnit = 32,
                                          blend = "Additive", layer = 2 })
  self.bullets:setHitTeams({ 2 })
  self.bullets:onHit(function(target, damage)
    local enemy = target:getScript("Enemy")
    if enemy then
      enemy:damage(damage)
    end
  end)

  self.lifeStyle = Sprites.makeStyle{ texture = "T_Ship", pixelsPerUnit = 64, layer = 10 }
end

function PlayerShip:update(dt)
  if self.health <= 0 then
    return
  end

  -- Move gives x right and y up, which on the 2D plane are world Y and Z.
  local move = Input.getActionValue2D("Move", self.move)
  local position = self.transform:getWorldPosition(self.position)
  local y = math.max(-self.halfWidth, math.min(self.halfWidth, position.y + move.x * self.speed * dt))
  local z = math.max(-self.halfHeight, math.min(self.halfHeight, position.z + move.y * self.speed * dt))
  self.transform:setWorldPosition(0, y, z)

  self.cooldown = self.cooldown - dt
  if Input.isActionDown("Fire") and self.cooldown <= 0 then
    -- Three shots fanned over 12 degrees around straight up (90).
    self.bullets:fireSpread(y, z + 0.5, 3, 90, 12)
    self.cooldown = self.fireInterval
  end

  if self.flash > 0 then
    self.flash = self.flash - dt
    local red = math.floor(self.flash * 20) % 2 == 0 and self.flash > 0
    self.sprite:setColor(red and Color(1, 0.35, 0.35, 1) or Color(1, 1, 1, 1))
  end

  -- Drawn every frame without GameObjects.
  for i = 1, self.health do
    Sprites.draw(self.lifeStyle, -self.halfWidth + (i - 0.5) * 0.7, self.halfHeight + 0.2)
  end
end

-- A hit while flashing is ignored, so one burst does not take every life at once.
function PlayerShip:damage(amount)
  if self.health <= 0 or self.flash > 0 then
    return
  end
  -- Damage arrives as a number; lives stay whole.
  self.health = self.health - math.max(1, math.floor(amount))
  self.flash = 0.8
  local position = self.transform:getWorldPosition(self.position)
  Game:explode(position.y, position.z, 10)
  if self.health <= 0 then
    self.sprite:setVisible(false)
    Game:explode(position.y, position.z, 48)
    Game:gameOver()
  end
end

return PlayerShip
