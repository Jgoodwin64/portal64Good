--- @module sk_scene

local node_name_cache = nil

local exports = {}

-- Defintions from LuaScene.cpp

---A scene node
---@table Node
---@tfield string name
---@tfield sk_transform.Transform transformation
---@tfield sk_transform.Transform full_transformation
---@tfield Node parent
---@tfield {Node,...} children
---@tfield {LuaMesh.Mesh,...} meshes 

--- Generates mesh and animation data from the current scene
---@function export_default_mesh
---@treturn sk_definition_writer.RawType model
---@treturn sk_definition_writer.RawType material
exports.export_default_mesh = function ()
    -- implmentation overridden by LuaScene.cpp
end

-- Defintions from LuaNodeGroups.cpp

---A pairing of nodes and pre parsed node arguments
---@table NodeWithArguments
---@tfield Node node
---@tfield {string,...} arguments the list is strings produced by splitting the node name by spaces

---Returns a list of nodes with the given string prefix
---@function nodes_for_type
---@tparam string prefix the string prefix to search
---@treturn NodeWithArguments
exports.nodes_for_type = function(prefix)
    
end

---Finds a named value in a list of arguments
---@function find_named_argument
---@tfield {string,...} arg_list
---@tfield string name
---@treturn string|nil
local function find_named_argument(arg_list, name)
    for i = 1,#arg_list do
        if (arg_list[i] == name) then
            return arg_list[i + 1]
        end
    end

    return nil
end

exports.find_named_argument = find_named_argument


---Finds a named value in a list of arguments
---@function find_flag_argument
---@tfield {string,...} arg_list
---@tfield string name
---@treturn boolean
local function find_flag_argument(arg_list, name)
    for i = 1,#arg_list do
        if (arg_list[i] == name) then
            return true
        end
    end

    return false
end

exports.find_flag_argument = find_flag_argument

local function build_node_with_name_cache(node)
    node_name_cache[node.name] = node

    for _, child in pairs(node.children) do
        build_node_with_name_cache(child)
    end
end

--- Returns a node with the given name
---@function node_with_name
---@tparam string name
---@treturn Node result
exports.node_with_name = function (name)
    if (not node_name_cache) then
        node_name_cache = {}
        build_node_with_name_cache(scene.root)
    end

    return node_name_cache[name]
end

return exports