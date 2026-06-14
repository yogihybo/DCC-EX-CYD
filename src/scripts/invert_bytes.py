import sys

filepath = r'c:\Users\Caleb Smith\Documents\GitHub\EX-T3-WiFi\src\dcc_icon.c'
with open(filepath, 'r') as f:
    lines = f.readlines()

new_lines = []
in_array = False
for line in lines:
    if 'dcc_icon_map[]' in line:
        in_array = True
        new_lines.append(line)
        continue
    if in_array and '};' in line:
        in_array = False
        new_lines.append(line)
        continue
    
    if in_array:
        parts = line.split(',')
        new_parts = []
        for p in parts:
            p = p.strip()
            if p.startswith('0x'):
                val = int(p, 16)
                inv_val = 255 - val
                new_parts.append(f'0x{inv_val:02x}')
            else:
                if p:
                    new_parts.append(p)
        # Re-add trailing comma if there were elements
        if new_parts:
            new_lines.append('  ' + ', '.join(new_parts) + ', \n')
        else:
            new_lines.append(line)
    else:
        new_lines.append(line)

with open(filepath, 'w') as f:
    f.writelines(new_lines)
print("Inverted bytes successfully.")
