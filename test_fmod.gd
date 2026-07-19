extends Node

var events = []

func _ready():
    print("TESTING FMOD LOAD BANK WITH FLAG 0...")
    FmodServer.load_bank("res://Audio/Master.strings.bank", 0)
    FmodServer.load_bank("res://Audio/Master.bank", 0)
    FmodServer.load_bank("res://Audio/Vehicles.bank", 0)
    
    var descs = FmodServer.get_all_event_descriptions()
    print("FOUND ", descs.size(), " EVENTS")
    
    for d in descs:
        var instance = FmodServer.create_event_instance_from_description(d)
        if instance:
            events.append(instance)
            instance.start()
            
func _process(delta):
    FmodServer.update()
    var t = Transform3D()
    if FmodServer.has_method("set_listener_transform3d"):
        FmodServer.set_listener_transform3d(0, t)
        
    for instance in events:
        instance.set_parameter_by_name("RPM", 5000.0)
        instance.set_parameter_by_name("Load", 1.0)
        if instance.has_method("set_3d_attributes"):
            instance.set_3d_attributes(t)
