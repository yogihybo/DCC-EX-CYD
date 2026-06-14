Import("env")
import csv
import os

def get_website_offset():
    try:
        with open('partitions.csv', 'r') as f:
            reader = csv.reader(f)
            for row in reader:
                if not row or row[0].strip().startswith('#'): continue
                if row[0].strip() == 'website':
                    return row[3].strip()
    except Exception:
        pass
    return None

offset = get_website_offset()

if offset:
    # Use the port if it's set in the environment or args, else let esptool auto-detect
    port_arg = '--port "$UPLOAD_PORT"' if 'UPLOAD_PORT' in env else ''
    
    env.AddCustomTarget(
        name="upload_website",
        dependencies=None,
        actions=[
            "pio run -t buildfs",
            f"pio pkg exec -p tool-esptoolpy -- esptool.py --chip esp32 {port_arg} --baud $UPLOAD_SPEED write_flash {offset} \"$BUILD_DIR/littlefs.bin\""
        ],
        title="Upload Website FS",
        description="Builds and uploads the website filesystem to the correct partition"
    )
