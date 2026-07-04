extends Node3D

var time = 0.0

func _process(delta):
	time += delta
	
	# Reference the SuperCar node and apply a subtle hover effect
	var supercar = get_node_or_null("SuperCar")
	if supercar:
		supercar.position.y = 0.4 + 0.05 * sin(time * 2.5)
		supercar.rotation.y = time * 0.4  # Slow, elegant rotation for showcase
		supercar.rotation.z = 0.03 * sin(time * 2.0) # Subtle banking
		
		# Spin the wheels around their local Y-axis (cylinder axis)
		for wheel_name in ["FrontLeftWheel", "FrontRightWheel", "RearLeftWheel", "RearRightWheel"]:
			var wheel = supercar.get_node_or_null(wheel_name)
			if wheel:
				wheel.rotate_object_local(Vector3(0, 1, 0), delta * 8.0)
