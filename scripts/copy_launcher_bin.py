Import("env")

import os
import shutil

# Local SD / M5Launcher install folder (project-relative)
LAUNCHER_DIR_NAME = "launcher"
BIN_NAME = "MusicGoGoGo.bin"

# Shared M5Stack firmware stash on this machine (also copied on each cardputer-adv build)
M5STACK_BIN_DIR = os.path.expanduser("~/ESP32/M5StackBin")


def copy_launcher_bin(source, target, env):
    firmware = str(target[0])
    if not os.path.isfile(firmware):
        return

    project_dir = env.subst("$PROJECT_DIR")
    launcher_dir = os.path.join(project_dir, LAUNCHER_DIR_NAME)
    os.makedirs(launcher_dir, exist_ok=True)

    launcher_dest = os.path.join(launcher_dir, BIN_NAME)
    shutil.copy2(firmware, launcher_dest)
    print("Launcher bin copied -> %s" % launcher_dest)

    os.makedirs(M5STACK_BIN_DIR, exist_ok=True)
    stash_dest = os.path.join(M5STACK_BIN_DIR, BIN_NAME)
    shutil.copy2(firmware, stash_dest)
    print("M5Stack bin stash copied -> %s" % stash_dest)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_launcher_bin)
