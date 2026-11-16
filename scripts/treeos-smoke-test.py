#!/usr/bin/python3
"""TreeOS 核心冒烟测试

对每个"有 ROM 的系统"用默认核心启动 15 秒，判断：
  - 进程崩溃（segfault/SIGILL）
  - 0 帧（黑屏/核心没跑起来）
  - 正常（有帧输出）

用法（设备上）：
  sudo python3 treeos-smoke-test.py            # 测试所有有 ROM 的系统
  sudo python3 treeos-smoke-test.py gba n64    # 只测指定系统
结果写到 /tmp/treeos-smoke.tsv，并打印表格。
测试期间会临时停止 EmulationStation，结束后自动恢复。
"""
import os
import re
import signal
import subprocess
import sys
import time

ES_CFG = "/etc/emulationstation/es_systems.cfg"
CORES = "/home/tree/.config/retroarch/cores"
RA = "/usr/bin/retroarch"
RA_CFG = "/home/ark/.config/retroarch/retroarch.cfg"
ROMROOT = "/roms"
TIMEOUT = 15

SKIP = {"ports", "videos", "flash", "pico-8"}  # 非 libretro 或特殊运行时


def parse_systems():
    s = open(ES_CFG).read()
    out = {}
    for m in re.finditer(r"<system>\s*<name>([^<]+)</name>(.*?)</system>", s, re.S):
        name, blk = m.group(1), m.group(2)
        ext = re.search(r"<extension>([^<]*)</extension>", blk)
        core = None
        em = re.search(r'<emulator name="retroarch">\s*<cores>\s*<core>([^<]+)</core>', blk, re.S)
        if em:
            core = em.group(1)
        out[name] = {
            "ext": [e.lower() for e in (ext.group(1).split() if ext else [])],
            "core": core,
        }
    return out


def pick_rom(system, exts):
    d = os.path.join(ROMROOT, system)
    if not os.path.isdir(d):
        return None
    for f in sorted(os.listdir(d)):
        if f.startswith("."):
            continue
        p = os.path.join(d, f)
        if os.path.isfile(p) and os.path.splitext(f)[1].lower() in exts:
            return p
    return None


def run_one(system, core, rom):
    core_path = os.path.join(CORES, core + "_libretro.so")
    if not os.path.exists(core_path):
        return "SKIP(no core)", ""
    env = dict(os.environ)
    env["XDG_RUNTIME_DIR"] = "/run/user/1002"
    env.pop("LD_LIBRARY_PATH", None)
    log = "/tmp/smoke-%s.log" % system
    with open(log, "w") as fh:
        p = subprocess.Popen(
            [RA, "-c", RA_CFG, "--verbose", "-L", core_path, rom],
            stdout=fh, stderr=subprocess.STDOUT, env=env, preexec_fn=os.setsid)
        time.sleep(TIMEOUT)
        alive = p.poll() is None
        if alive:
            os.killpg(p.pid, signal.SIGINT)
            time.sleep(2)
            if p.poll() is None:
                os.killpg(p.pid, signal.SIGKILL)
        code = p.poll()
    text = open(log, errors="ignore").read()
    frames = re.search(r"Frames pushed:\s*(\d+)", text)
    crash = ("Segmentation fault" in text or "Illegal instruction" in text
             or (code is not None and code < 0))
    if crash:
        return "CRASH(code=%s)" % code, text
    if frames and int(frames.group(1)) == 0:
        return "BLACK(0 frames)", text
    if not frames and code not in (0, None):
        return "EXIT(%s)" % code, text
    return "OK", text


def main():
    only = set(sys.argv[1:])
    systems = parse_systems()
    subprocess.run(["systemctl", "stop", "treeos-emulationstation"],
                   check=False, capture_output=True)
    time.sleep(2)
    rows = []
    try:
        for name, info in sorted(systems.items()):
            if only and name not in only:
                continue
            if name in SKIP or not info["core"]:
                continue
            rom = pick_rom(name, info["ext"])
            if not rom:
                continue
            status, text = run_one(name, info["core"], rom)
            err = ""
            if status != "OK":
                m = re.search(r"\[ERROR\][^\n]+", text)
                err = m.group(0)[:120] if m else ""
            rows.append((name, info["core"], os.path.basename(rom), status, err))
            print("%-10s %-28s %-8s %s" % (name, info["core"], status, err))
    finally:
        subprocess.run(["systemctl", "start", "treeos-emulationstation"],
                       check=False, capture_output=True)
    with open("/tmp/treeos-smoke.tsv", "w") as f:
        f.write("system\tcore\trom\tstatus\terror\n")
        for r in rows:
            f.write("\t".join(r) + "\n")
    print("\n结果已写入 /tmp/treeos-smoke.tsv")


if __name__ == "__main__":
    main()
