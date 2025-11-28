#!python3

"""
1. Download (raw) image from a url.
2. Draw PiSugar battery bar to the image (at top-right).
3. Paint the image via epd-paint, if image changed.
4. Shutdown if at midnight.
"""

import os
import hashlib
import sys
import urllib.request
import subprocess
import socket
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
    exe_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), "epd-paint")
    proc = subprocess.Popen([exe_path], stdin=subprocess.PIPE)
    proc.stdin.write(image)
    proc.stdin.close()
    proc.wait()


def shutdown():
    print("Shutting down")
    os.system('sudo shutdown -h now "Completed paint."')


def main():
    battery_level = get_pisugar_battery_level()
    print("Battery level: %.2f" % battery_level)

    url = "http://iot.home/13.sp6"
    image = download_file_to_memory(url)
    if len(image) != 1600 * 1200 / 2:
        print("Wrong image size")
        return
    print("Downloaded image from %s" % url)

    image = draw_battery(image, battery_level)
    if image_changed(image):
        print("Paint (changed) image")
        paint_image(image)
    else:
        print("Skip painting - no change")

    charging = get_pisugar_is_charging()
    if charging:
        print("Skip shutdown - is charging")
    else:
        shutdown()


if __name__ == "__main__":
    main()
