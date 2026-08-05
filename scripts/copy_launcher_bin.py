Import("env")

import os
import shutil


def copy_launcher_bin(source, target, env):
    firmware = str(target[0])
    if not os.path.isfile(firmware):
        return

    dest_dir = os.path.join(env.subst("$PROJECT_DIR"), "launcher")
    os.makedirs(dest_dir, exist_ok=True)

    dest = os.path.join(dest_dir, "MusicGoGoGo.bin")
    shutil.copy2(firmware, dest)
    print("Launcher bin copied -> %s" % dest)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_launcher_bin)
