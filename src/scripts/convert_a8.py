import sys

filepath = r'c:\Users\Caleb Smith\Documents\GitHub\EX-T3-WiFi\src\dcc_icon.c'
with open(filepath, 'r') as f:
    lines = f.readlines()

new_lines = []
in_array = False
hex_bytes = []

# First pass: extract all bytes
for line in lines:
    if 'dcc_icon_map[]' in line:
        in_array = True
        new_lines.append(line)
        continue
    if in_array and '};' in line:
        in_array = False
        break
    
    if in_array:
        parts = line.split(',')
        for p in parts:
            p = p.strip()
            if p.startswith('0x'):
                hex_bytes.append(int(p, 16))

if len(hex_bytes) != 3600:
    print(f"Expected 3600 bytes for 30x30 ARGB8888, but got {len(hex_bytes)}")

# Convert to A8
a8_bytes = []
for i in range(0, len(hex_bytes), 4):
    # hex_bytes is [B, G, R, A] or [R, G, B, A]
    # We'll just use the first byte (since it's grayscale)
    color_val = hex_bytes[i] 
    # Invert to make dark pixels opaque, light pixels transparent
    alpha_val = 255 - color_val
    a8_bytes.append(alpha_val)

# Reconstruct file
out_lines = []
in_array = False
in_struct = False
for line in lines:
    if 'dcc_icon_map[]' in line:
        in_array = True
        out_lines.append(line)
        # Write new array
        for i in range(0, len(a8_bytes), 15):
            chunk = a8_bytes[i:i+15]
            out_lines.append('  ' + ', '.join(f'0x{b:02x}' for b in chunk) + ',\n')
        continue
        
    if in_array:
        if '};' in line:
            in_array = False
            out_lines.append(line)
        continue
        
    if 'const lv_image_dsc_t dcc_icon =' in line:
        in_struct = True
        out_lines.append(line)
        continue
        
    if in_struct:
        if '.header.cf' in line:
            out_lines.append('  .header.cf = LV_COLOR_FORMAT_A8,\n')
            continue
        if '.data_size' in line:
            out_lines.append('  .data_size = 900,\n')
            continue
        if '.header.h' in line:
            out_lines.append(line)
            out_lines.append('  .header.stride = 30,\n')
            continue
        if '};' in line:
            in_struct = False
            
    out_lines.append(line)

with open(filepath, 'w') as f:
    f.writelines(out_lines)

print("Conversion complete!")
