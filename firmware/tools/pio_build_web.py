# Script previo de PlatformIO: regenera los recursos web embebidos si se ha
# editado algo en firmware/web/ o firmware/layouts/.
Import("env")  # noqa: F821
import subprocess
import sys

subprocess.check_call([sys.executable, "tools/build_web.py"], cwd=env.subst("$PROJECT_DIR"))  # noqa: F821
