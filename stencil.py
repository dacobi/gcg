import os
import math

try:
    from PIL import Image, ImageDraw
except ImportError:
    import sys
    import subprocess
    subprocess.check_call([sys.executable, "-m", "pip", "install", "Pillow"])
    from PIL import Image, ImageDraw

# Set up dimensions
width, height = 512, 512
img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
pixels = img.load()

# 1. Generate the coordinates of the heart boundary
heart_points = []
for i in range(0, 360):
    t = math.radians(i)
    # Parametric equations for the heart shape
    x = 16 * (math.sin(t) ** 3)
    y = 13 * math.cos(t) - 5 * math.cos(2*t) - 2 * math.cos(3*t) - math.cos(4*t)
    
    # Scale and center on canvas
    pixel_x = int(256 + x * 13)
    pixel_y = int(256 - y * 11)
    heart_points.append((pixel_x, pixel_y))

# 2. Create an internal mask to quickly check what is inside the heart
mask_img = Image.new("L", (width, height), 0)
draw = ImageDraw.Draw(mask_img)
draw.polygon(heart_points, fill=255)

# 3. Process every single pixel for a perfectly smooth alpha gradient outward
# Controls how many pixels wide the glow/fade is before dropping to 0% opacity
FADE_RADIUS = 160.0 

for y in range(height):
    for x in range(width):
        # If the pixel is inside the heart, keep it completely solid
        if mask_img.getpixel((x, y)) == 255:
            pixels[x, y] = (255, 255, 255, 255)
        else:
            # Calculate shortest math distance to the heart's outer edge
            min_sq_dist = min((x - px)**2 + (y - py)**2 for px, py in heart_points)
            dist_to_heart = math.sqrt(min_sq_dist)
            
            if dist_to_heart >= FADE_RADIUS:
                alpha = 0
            else:
                # Linear falloff calculation: 255 at edge down to 0 at the limit
                alpha = int(255 * (1.0 - (dist_to_heart / FADE_RADIUS)))
            
            # Map the color value with the falling alpha mask
            if alpha > 0:
                pixels[x, y] = (255, 255, 255, alpha)

# Save directly to your desktop
desktop = os.path.join(os.path.expanduser("~"), "Desktop")
output_path = os.path.join(desktop, "faded_transparent_heart.png")
img.save(output_path, "PNG")

print(f"Success! Gradient transparent PNG created on your Desktop at: {output_path}")

