#!/usr/bin/env python3

# crontab -e
# * * * * * src/homeepd/dev-133e6/cronjob/maybe_shutdown.py >> /dev/null 2>&1

import os
import socket
import subprocess


def ask_pisugar(message: str) -> str:
    HOST = "127.0.0.1"
    PORT = 8423
    with socket.create_connection((HOST, PORT), timeout=5) as s:
        s.sendall(message.encode())
        data = s.recv(4096)
        return data.decode().split(":", 1)[-1].strip()


def get_pisugar_is_charging() -> bool:
    res = ask_pisugar("get battery_power_plugged\n")
    return res.strip() == "true"


def get_uptime_seconds():
    with open("/proc/uptime", "r") as f:
        return float(f.readline().split()[0])


def has_ssh_connections():
    sessions_dir = "/run/systemd/sessions"
    if not os.path.isdir(sessions_dir):
        return False

    for filename in os.listdir(sessions_dir):
        path = os.path.join(sessions_dir, filename)
        try:
            with open(path, "r") as f:
                content = f.read()
            if "SERVICE=sshd" in content.splitlines():
                return True
        except Exception:
            continue
    return False


def main():
    uptime = get_uptime_seconds()
    reasons = []
    uptime_limit = 118
    if uptime < uptime_limit:
        reasons.append(f"uptime {uptime} < {uptime_limit}")
    if has_ssh_connections():
        reasons.append("has ssh connections")
    if get_pisugar_is_charging():
        reasons.append("PiSugar is charging")
    if reasons:
        print("Skip shutdown:")
        for reason in reasons:
            print(f"- {reason}")
    else:
        print("Shutting down...")
        os.system("sudo shutdown -h now 'Auto shutdown with no connections.'")


if __name__ == "__main__":
    main()
