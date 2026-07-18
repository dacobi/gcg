-- Add a Godot bouncer using the empty scene
addBouncer("[tscn:empty.tscn]")
-- We wait a moment to ensure it is fully instantiated in the backend
delay(100)

-- Select the first Godot bouncer (index 0)
selectGodot(0)

-- Test Lua Mutex
print("Testing Lua Mutex...")
local locked = luaGetMutex()
print("Mutex locked: ", locked)
if locked then
    luaReleaseMutex()
    print("Mutex released")
end

-- Test Godot Direct Node Pointers
print("Testing Direct Node Pointers...")

-- Select the root of the selected bouncer
godotSelectRoot()
print("Selected root!")

-- Create two child nodes
local r1 = godotCreateNode("TestNode1")
print("Create TestNode1: ", r1)

godotSelectRoot()
print("Selected root!")


local r2 = godotCreateNode("TestNode2")
print("Create TestNode2: ", r2)

godotSelectRoot()
print("Selected root!")


local node1 = godotGetNodePointer("TestNode1")


godotSelectRoot()
print("Selected root!")




local node2 = godotGetNodePointer("TestNode2")

print("Node1 pointer: ", node1)
print("Node2 pointer: ", node2)

if node1 and node2 then
    -- Modify node1 directly
    godotSetPos(10, 20, 30, node1)
   
    godotSetPos(20, 40, 60, node2)
    

    -- Verify positions
    local px, py, pz = godotGetPos(node1)
    print("Node1 pos: ", px, py, pz)
    
    local p2x, p2y, p2z = godotGetPos(node2)
    print("Node2 pos: ", p2x, p2y, p2z)
end

delayKb(22000)

print("Test complete.")
appQuit()
