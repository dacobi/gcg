extends Node

func _ready():
    print("TESTING FMOD LOAD BANK...")
    print("Trying load_bank res://Audio/Master.strings.bank")
    FmodServer.load_bank("res://Audio/Master.strings.bank", 0)
    FmodServer.load_bank("res://Audio/Master.bank", 0)
    FmodServer.load_bank("res://Audio/Vehicles.bank", 0)
    
    var events = FmodServer.get_all_event_descriptions()
    print("FMOD EVENTS SIZE: ", events.size())
    for e in events:
        print("FMOD EVENT: ", e.get_path())
    
    print("TEST COMPLETE")
    get_tree().quit()
