import subprocess
import datetime
Import("env")

def get_firmware_version():
    try:
        version = subprocess.check_output(
            ["git", "describe", "--tags", "--dirty", "--always"],
            stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        version = "dev"
    return version

def get_build_timestamp():
    return datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")

version = get_firmware_version()
timestamp = get_build_timestamp()
firmware_name = env.get("PIOENV", "inversa")

env.Append(
    BUILD_FLAGS=[
        f'-D BUILD_GIT_VERSION=\\"{version}\\"',
        f'-D BUILD_TIMESTAMP=\\"{timestamp}\\"',
        f'-D FIRMWARE_NAME=\\"{firmware_name}\\"',
    ]
)

print(f"  Firmware: {firmware_name} @ {version} ({timestamp})")
