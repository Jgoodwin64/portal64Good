
Quaternion = {}

function quaternion(x, y, z, w) 
    return setmetatable({ x = x, y = y, z = z, w = w }, Quaternion)
end