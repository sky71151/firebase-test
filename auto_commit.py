
# Zorg dat PlatformIO de after_build functie daadwerkelijk aanroept
try:
    Import("env")
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", after_build)
except Exception as e:
    print(f"[auto_commit.py] Kon SCons hook niet registreren: {e}")
# Auto-commit script for PlatformIO post-build
import os
import subprocess
import datetime

print("[auto_commit.py] Script wordt geladen door PlatformIO")

def after_build(source, target, env):
    print("[auto_commit.py] Script gestart!")
    repo_dir = env['PROJECT_DIR']
    version_file = os.path.join(repo_dir, "firmware_version.txt")
    version = "unknown"
    if os.path.exists(version_file):
        with open(version_file, "r") as vf:
            version = vf.read().strip()
    print(f"[auto_commit.py] Firmware versie: {version}")
    os.chdir(repo_dir)
    try:
        print("[auto_commit.py] git add -A")
        subprocess.run(["git", "add", "."], check=True)
        msg = f"Auto-commit: build {version}"
        print(f"[auto_commit.py] git commit -m '{msg}'")
        subprocess.run(["git", "commit", "-m", msg], check=True)
    except subprocess.CalledProcessError as e:
        print(f"[auto_commit.py] Git commit failed: {e}")

Import("env")
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", after_build)
