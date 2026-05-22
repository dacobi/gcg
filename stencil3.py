import os
import math

try:
    from PIL import Image, ImageDraw
except ImportError:
    import sys
    import subprocess
    subprocess.check_call([sys.executable, "-m", "pip", "install", "Pillow"])
    from PIL import Image, ImageDraw

def generate_shape_points(shape_type, center_x=256, center_y=256):
    points = []
    
    if shape_type == "heart":
        for i in range(0, 360):
            t = math.radians(i)
            x = 16 * (math.sin(t) ** 3)
            y = 13 * math.cos(t) - 5 * math.cos(2*t) - 2 * math.cos(3*t) - math.cos(4*t)
            pixel_x = int(center_x + x * 13)
            pixel_y = int(center_y - y * 11)
            points.append((pixel_x, pixel_y))
            
    elif shape_type == "star":
        # 5-pointed star geometry
        outer_radius = 200
        inner_radius = 80
        # 10 vertices total (5 outer, 5 inner)
        for i in range(10):
            angle = math.radians(i * 36 - 90)  # Offset by -90 to point straight up
            r = outer_radius if i % 2 == 0 else inner_radius
            pixel_x = int(center_x + r * math.cos(angle))
            pixel_y = int(center_y + r * math.sin(angle))
            points.append((pixel_x, pixel_y))
            
    elif shape_type == "moon":
        # Outer arc of the crescent moon (clockwise)
        outer_radius = 200
        for angle_deg in range(-110, 111, 2):
            angle = math.radians(angle_deg)
            pixel_x = int(center_x + outer_radius * math.cos(angle))
            pixel_y = int(center_y + outer_radius * math.sin(angle))
            points.append((pixel_x, pixel_y))
        
        # Inner arc of the crescent moon (counter-clockwise back to start)
        inner_radius = 175
        offset_x = 90  # Shifts inner circle rightward to create the crescent cut
        for angle_deg in range(85, -86, -2):
            angle = math.radians(angle_deg)
            pixel_x = int(center_x - offset_x + inner_radius * math.cos(angle))
            pixel_y = int(center_y + inner_radius * math.sin(angle))
            points.append((pixel_x, pixel_y))
            
    return points

def create_faded_image(shape_name):
    width, height = 512, 512
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    pixels = img.load()
    
    # 1. Generate path mapping
    shape_points = generate_shape_points(shape_name)
    
    # 2. Build the primary solid geometry mask
    mask_img = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(mask_img)
    draw.polygon(shape_points, fill=255)
    
    # 3. Calculate distance fields and output white pixel arrays
    FADE_RADIUS = 55
    
    for y in range(height):
        for x in range(width):
            if mask_img.getpixel((x, y)) == 255:
                pixels[x, y] = (255, 255, 255, 255)
            else:
                min_sq_dist = min((x - px)**2 + (y - py)**2 for px, py in shape_points)
                dist_to_edge = math.sqrt(min_sq_dist)
                
                if dist_to_edge >= FADE_RADIUS:
                    alpha = 0
                else:
                    alpha = int(255 * (1.0 - (dist_to_edge / FADE_RADIUS)))
                
                if alpha > 0:
                    pixels[x, y] = (255, 255, 255, alpha)
                    
    # Save file
    desktop = os.path.join(os.path.expanduser("~"), "Desktop")
    filename = f"fast_fade_white_{shape_name}.png"
    output_path = os.path.join(desktop, filename)
    img.save(output_path, "PNG")
    print(f"Generated: {output_path}")

# Run pipeline for all three assets
shapes = ["heart", "star", "moon"]
for shape in shapes:
    create_faded_image(shape)

