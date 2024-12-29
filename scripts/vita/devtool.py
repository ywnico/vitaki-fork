#!/usr/bin/env python3
import argparse
import re
import socket
import sys
import tempfile
import time
import zipfile
from pathlib import Path
from ftplib import FTP, error_perm, error_reply
from typing import Optional
from collections import defaultdict

path_root = Path(__file__).parents[0]
sys.path.insert(0, ".")
sys.path.insert(0, str(path_root) + "/parse_core")

from parse_core.core import CoreParser
from parse_core.elf import ElfParser
from parse_core.util import u16, u32, c_str, hexdump
from parse_core.indent import iprint, indent
# from parse_core.main import print_thread_info, print_module_info

COLOR_RED = '\x1b[31;1m'
COLOR_BLUE = '\x1b[34;1m'
COLOR_END = '\x1b[0m'

LOG_LEVEL_COLORS = {
    'DEBUG': '',
    'INFO': COLOR_BLUE,
    'ERROR': COLOR_RED,
}

LOG_LINE_PAT = re.compile(
    r'^\[(?P<level>[A-Z]+?)\]: (?P<is_chiaki>\[CHIAKI\] )?(?P<timestamp>\d+) (?P<msg>.+)$')

def fetch_latest_coredump(hostname: str) -> Optional[Path]:
    ftp = FTP()
    ftp.connect(hostname, 1337)
    ftp.login()
    fnames = []
    ftp.cwd("/ux0:/data")
    ftp.retrlines("LIST", lambda l: fnames.append(l.split(" ")[-1]))
    coredumps = sorted(fname for fname in fnames if fname.endswith(".psp2dmp"))
    if not coredumps:
        return None
    corepath = tempfile.mkstemp(prefix="chiaki-vitadevtool")[1]
    with open(corepath, "wb") as fp:
        ftp.retrbinary(f"RETR {coredumps[-1]}", callback=fp.write)
    for old_core in coredumps[:-1]:
        try:
            ftp.delete(old_core)
        except error_reply as e:
            if e.args[0].startswith('226 '):
                continue
            raise e
    return Path(corepath)

str_stop_reason = defaultdict(str, {
    0: "No reason",
    0x30002: "Undefined instruction exception",
    0x30003: "Prefetch abort exception",
    0x30004: "Data abort exception",
    0x60080: "Division by zero",
})

str_status = defaultdict(str, {
    1: "Running",
    8: "Waiting",
    16: "Not started",
})

str_attr = defaultdict(str, {
    5: "RX",
    6: "RW",
})

reg_names = {
    13: "SP",
    14: "LR",
    15: "PC",
}

isPC = True

def print_module_info(module):
    iprint(module.name)
    with indent():
        for x, segment in enumerate(module.segments):
            iprint("Segment {}".format(x + 1))
            with indent():
                iprint("Start: 0x{:x}".format(segment.start))
                iprint("Size: 0x{:x} bytes".format(segment.size))
                iprint("Attributes: 0x{:x} ({})".format(segment.attr, str_attr[segment.attr & 0xF]))
                iprint("Alignment: 0x{:x}".format(segment.align))


def print_thread_info(thread, core=None, elf=None):
    iprint(thread.name)
    with indent():
        iprint("ID: 0x{:x}".format(thread.uid))
        iprint("Stop reason: 0x{:x} ({})".format(thread.stop_reason, str_stop_reason[thread.stop_reason]))
        iprint("Status: 0x{:x} ({})".format(thread.status, str_status[thread.status]))
        pc = core.get_address_notation("PC", thread.pc)
        iprint(pc.to_string(elf))
        if not pc.is_located():
            iprint(core.get_address_notation("LR", thread.regs.gpr[14]).to_string(elf))


def print_coredump(corepath: Path) -> None:
    elfpath = Path(__file__).parent / '../../build/vita/Vitaki.elf'
    if not elfpath.exists():
        print('Unable to find ELF file, please build the app and trigger '
              'the crash that caused the coredump again on your Vita.',
              file=sys.stderr)
        sys.exit(1)
    elf = ElfParser(str(elfpath))
    core = CoreParser(str(corepath))
    iprint("=== THREADS ===")
    crashed = []
    with indent():
        for thread in core.threads:
            if thread.stop_reason != 0:
                crashed.append(thread)
            print_thread_info(thread, core, elf)
    iprint()
    for thread in crashed:
        iprint('=== THREAD "{}" <0x{:x}> CRASHED ({}) ==='.format(thread.name, thread.uid, str_stop_reason[thread.stop_reason]))

        pc = core.get_address_notation('PC', thread.pc)
        pc.print_disas_if_available(elf)
        lr = core.get_address_notation('LR', thread.regs.gpr[14])
        lr.print_disas_if_available(elf)

        iprint("REGISTERS:")
        with indent():
            for x in range(14):
                reg = reg_names.get(x, "R{}".format(x))
                iprint("{}: 0x{:x}".format(reg, thread.regs.gpr[x]))

            iprint(pc)
            iprint(lr)
                
                
        iprint()

        iprint("STACK CONTENTS AROUND SP:")
        with indent():
            sp = thread.regs.gpr[13]
            for x in range(-16, 24):
                addr = 4 * x + sp
                data = core.read_vaddr(addr, 4)
                if data:
                    data = u32(data, 0)
                    prefix = "     "
                    if addr == sp:
                        prefix = "SP =>"
                    data_notation = core.get_address_notation("{} 0x{:x}".format(prefix, addr), data)
                    iprint(data_notation.to_string(elf))


def _vitacompanion_send_cmd(hostname: str, cmd: str) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((hostname, 1338))
    sock.sendall(f'{cmd}\n'.encode('utf8'))


def run_app(hostname: str) -> None:
    # _vitacompanion_send_cmd(hostname, 'destroy')
    # time.sleep(0.2)
    _vitacompanion_send_cmd(hostname, 'launch CHIAKI001')


def upload_app(hostname: str, with_assets: bool) -> None:
    # Can't overwrite running app, must kill it first
    _vitacompanion_send_cmd(hostname, 'destroy')
    time.sleep(0.2)
    vpk_path = Path(__file__).parent / '../../build/vita/Vitaki.vpk'
    if not vpk_path.exists():
        print(COLOR_RED + "No Vita build found, please run the build script first." + COLOR_END)
        sys.exit(1)
    ftp = FTP()
    ftp.connect(hostname, 1337)
    ftp.login()
    try:
        ftp.cwd('ux0:/app/CHIAKI001')
    except error_perm as e:
        if e.args[0].startswith('550 '):
            print(COLOR_RED + "Vitaki is not installed, please perform an initial installation of the VPK first!" + COLOR_END)
            sys.exit(1)
        raise e
    with zipfile.ZipFile(vpk_path) as zf:
        for fpath in zf.namelist():
            is_asset = fpath.startswith('assets/') or fpath.startswith('sce_sys')
            if is_asset and not with_assets:
                continue
            print(fpath)
            with zf.open(fpath) as fp:
                parts = fpath.split('/')
                directory = '/'.join(parts[:-1])
                fname = parts[-1]
                if directory:
                    ftp.cwd(directory)
                ftp.storbinary(f'STOR {fname}', fp)
                if directory:
                    while not ftp.pwd().endswith('CHIAKI001'):
                        ftp.cwd('..')


def tail_logs(host: str) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('', 31338))
    fp = sock.makefile()
    while True:
        try:
            logline = ""
            try: 
                logline = fp.readline().replace('[VITA]', '')
            except UnicodeDecodeError:
                pass
            match = LOG_LINE_PAT.match(logline)
            if match is None:
                continue
            timestamp = int(match.group('timestamp')) / 1e6
            is_chiaki = match.group('is_chiaki') is not None
            msg = match.group('msg')
            level = match.group('level')
            color = LOG_LEVEL_COLORS[level]
            if is_chiaki:
                level = f'CHIAKI-{level}'
            level = f'[{level}]'
            print(f'{color}{timestamp:>8.4f} {level:<16} {msg}{COLOR_END}')
        except KeyboardInterrupt:
            return


def main():
    parser = argparse.ArgumentParser(
        description="Utilities to simplify debugging chiaki on a PS Vita."
    )
    parser.add_argument(
        "--host",
        action="store",
        dest="vita_host",
        help="IPv4 address of the PS Vita",
        required=True,
    )
    parser.add_argument(
        '--upload-assets',
        action='store_true',
        dest='upload_assets',
        help="Upload assets in addition to application binary",
        default=False
    )
    parser.add_argument("command", choices=["coredump", "upload", "launch"])
    args = parser.parse_args()
    if args.command == 'coredump':
        core_path = fetch_latest_coredump(args.vita_host)
        if core_path is None:
            print(COLOR_RED + 'Could not fetch latest coredump from device.' + COLOR_END)
            sys.exit(1)
        print_coredump(core_path)
    elif args.command == 'upload':
        upload_app(args.vita_host, args.upload_assets)
    elif args.command == 'launch':
        run_app(args.vita_host)
        # tail_logs(args.vita_host)


if __name__ == "__main__":
    main()
