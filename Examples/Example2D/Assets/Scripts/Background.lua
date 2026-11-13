-- chimera-uuid: 72002fc5-d6c8-4978-924b-9755605213b4
-- A star field in three layers that scroll at different speeds, drawn every frame with
-- Sprites.draw: no GameObject per star.
---@class Background : Script
---@field starCount integer
---@field halfWidth number
---@field halfHeight number
local Background = {}

Background.properties = {
  { "starCount", 160, type = "integer", min = 10, max = 5000 },
  { "halfWidth", 14.5, tooltip = "Half the width the stars cover" },
  { "halfHeight", 8.5, tooltip = "Half the height the stars cover" },
}

function Background:onStart()
  local function layer(speed, scale, brightness, blend)
    return {
      speed = speed,
      scale = scale,
      style = Sprites.makeStyle{ texture = "T_Star", pixelsPerUnit = 32, layer = -10,
                                 color = Color(brightness * 0.8, brightness * 0.85,
                                               brightness, 1),
                                 blend = blend },
    }
  end
  self.layers = {
    layer(0.6, 0.6, 0.45, "Alpha"),
    layer(1.4, 0.9, 0.75, "Alpha"),
    layer(3.0, 1.2, 1.0, "Additive"),
  }

  self.stars = {}
  for i = 1, self.starCount do
    self.stars[i] = {
      y = (math.random() * 2 - 1) * self.halfWidth,
      z = (math.random() * 2 - 1) * self.halfHeight,
      layer = self.layers[i % #self.layers + 1],
    }
  end
end

function Background:update(dt)
  local halfWidth, halfHeight = self.halfWidth, self.halfHeight
  local stars = self.stars
  for i = 1, #stars do
    local star = stars[i]
    local layer = star.layer
    star.z = star.z - layer.speed * dt
    if star.z < -halfHeight then
      star.z = star.z + halfHeight * 2
      star.y = (math.random() * 2 - 1) * halfWidth
    end
    Sprites.draw(layer.style, star.y, star.z, 0, layer.scale)
  end
end

return Background
