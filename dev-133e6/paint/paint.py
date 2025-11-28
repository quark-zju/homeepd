#!python3

"""
1. Download (raw) image from a url.
2. Draw PiSugar battery bar to the image (at top-right).
3. Paint the image via epd-paint, if image changed.
"""

# Maybe add " mmc_core.timeout=300" to /boot/firmware/cmdline.txt
# power consumption might force unmount the sd card...


import hashlib
import os
import socket
import subprocess
import sys
import time
import urllib.request
from datetime import datetime


def download_file_to_memory(url: str) -> bytes:
    with urllib.request.urlopen(url) as response:
        data = response.read()  # 读取全部二进制数据
    return data


def get_hash(data):
    return hashlib.blake2s(data).hexdigest()


def ask_pisugar(message: str) -> str:
    HOST = "127.0.0.1"
    PORT = 8423
    with socket.create_connection((HOST, PORT), timeout=5) as s:
        s.sendall(message.encode())
        data = s.recv(4096)
        return data.decode().split(":", 1)[-1].strip()


def get_pisugar_battery_level():
    res = ask_pisugar("get battery\n")
    return float(res)


def get_pisugar_is_charging() -> bool:
    res = ask_pisugar("get battery_power_plugged\n")
    return res.strip() == "true"


def draw_battery(image, battery_level):
    image = bytearray(image)
    WIDTH = 1200
    HEIGHT = 1600
    BAR_LEN = 50
    BAR_HEIGHT = 6
    WHITE = 1
    GREEN = 6

    def draw_pixel(x, y, color):
        pos = (y * WIDTH + x) // 2
        v = image[pos]
        if (x & 1) == 0:
            v = (v & 0x0F) | (color << 4)
        else:
            v = (v & 0xF0) | color
        image[pos] = v

    x0 = WIDTH - BAR_LEN - 10
    y0 = 8

    for y in range(BAR_HEIGHT + 2):
        for x in range(BAR_LEN + 2):
            draw_pixel(x + x0, y + y0, WHITE)

    x0 += 1
    y0 += 1
    for y in range(BAR_HEIGHT):
        for x in range(int(battery_level + 0.5) // 2):
            draw_pixel(x + x0, y + y0, GREEN)

    return image


def image_changed(image) -> bool:
    last_hash_path = os.path.join(
        os.path.dirname(os.path.realpath(__file__)), ".lasthash"
    )
    try:
        with open(last_hash_path, "r") as f:
            last_hash = f.read()
    except FileNotFoundError:
        last_hash = ""
    current_hash = get_hash(image)
    if current_hash == last_hash:
        return False
    else:
        with open(last_hash_path, "w") as f:
            f.write(current_hash)
        return True


def paint_image(image):
    os.system("sync")
    exe_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), "epd-paint")
    proc = subprocess.Popen([exe_path], stdin=subprocess.PIPE)
    proc.stdin.write(image)
    proc.stdin.close()
    proc.wait()


def get_shutdown_mode():
    url = "http://iot.home/shutdown.txt"
    try:
        mode = int(download_file_to_memory(url).decode())
    except Exception:
        mode = 0
    return mode


def has_ssh_connections():
    sessions_dir = "/run/systemd/sessions"
    if not os.path.isdir(sessions_dir):
        return False

    for filename in os.listdir(sessions_dir):
        if filename.endswith(".ref"):
            continue
        path = os.path.join(sessions_dir, filename)
        try:
            with open(path, "r") as f:
                content = f.read()
            if "SERVICE=sshd" in content.splitlines():
                return True
        except Exception:
            continue
    return False


sysrq = None
pisugar_poweroff = None
pisugar_config = None


def create_memfd(path: str) -> int:
    with open(path, "rb") as f:
        data = f.read()
    fd = os.memfd_create("prog", flags=0)
    os.write(fd, data)
    os.lseek(fd, 0, os.SEEK_SET)
    return fd


def collect_no_shutdown_reasons():
    reasons = []
    if has_ssh_connections():
        reasons.append("has ssh connections")
    if get_pisugar_is_charging():
        reasons.append("PiSugar is charging")
    mode = get_shutdown_mode()
    if mode != 0:
        reasons.append(f"shutdown mode {mode} != 0")
    global sysrq, pisugar_poweroff, pisugar_config
    try:
        sysrq = open("/proc/sysrq-trigger", "wb")
    except IOError:
        pass
    try:
        fd = create_memfd("/bin/pisugar-poweroff")
        # in case "/" becomes inaccessible...
        os.chdir("/proc/self")
        pisugar_poweroff = f"./fd/{fd}"
        fd = create_memfd("/etc/pisugar-server/config.json")
        pisugar_config = f"./fd/{fd}"
    except IOError:
        pass
    return reasons


def shutdown():
    print("Shutting down via 'shutdown'...")
    try:
        cmd = "/sbin/shutdown now"
        if os.geteuid() != 0:
            cmd = "/bin/sudo " + cmd
        os.system(cmd)
    except:
        pass
    if pisugar_poweroff:
        args = ["pisugar-poweroff", "--model", "PiSugar 3"]
        if pisugar_config:
            args += ["--config", pisugar_config]
        print(f"Shutting down via 'pisugar-poweroff': {args}")
        os.execv(pisugar_poweroff, args)
    if sysrq:
        print("Shutting down via 'sysrq'...")
        time.sleep(3)
        sysrq.write(b"s\nu\no\n")
        sysrq.flush()


def maybe_shutdown(reasons):
    if reasons:
        print("Skip shutdown:")
        for reason in reasons:
            print(f"- {reason}")
    else:
        shutdown()


def main():
    battery_level = get_pisugar_battery_level()
    print("Battery level: %.2f" % battery_level)

    url = "http://iot.home/13.sp6"
    image = download_file_to_memory(url)
    if len(image) != 1600 * 1200 / 2:
        print("Wrong image size")
        return
    print("Downloaded image from %s" % url)

    # Read reasons before the filesystem becomes unavailable...
    no_shutdown_reasons = collect_no_shutdown_reasons()

    image = draw_battery(image, battery_level)
    if image_changed(image):
        print("Paint (changed) image")
        if os.getenv("SKIP_PAINT"):
            print("  Skip with SKIP_PAINT")
        else:
            # This might draw power to make the mmc timeout...
            # and force the filesystem to crash...
            paint_image(image)
    else:
        print("Skip painting - no change")

    # At this time, the filesystem might be gone. Things like
    # /bin/python3 is unavailable. We want to shutdown without
    # depending on the fs.
    maybe_shutdown(no_shutdown_reasons)


if __name__ == "__main__":
    main()
