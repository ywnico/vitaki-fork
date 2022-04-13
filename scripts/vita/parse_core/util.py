from elftools.common.py3compat import str2bytes

import string
import struct


def u16(buf, off):
    buf = buf[off:off+2]
    try:
        buf = str2bytes(buf)
    except AttributeError:
        pass
    return struct.unpack("<H", buf)[0]


def u32(buf, off):
    buf = buf[off:off+4]
    try:
        buf = str2bytes(buf)
    except AttributeError:
        pass
    return struct.unpack("<I", buf)[0]


def c_str(buf, off):
    out = ""
    while off < len(buf) and buf[off] != '\0':
        out += chr(buf[off])
        off += 1
    return out


def hexdump(src, length=16, sep='.'):
    DISPLAY = string.digits + string.letters + string.punctuation
    FILTER = ''.join(((x if x in DISPLAY else '.') for x in map(chr, range(256))))
    lines = []
    for c in xrange(0, len(src), length):
        chars = src[c:c+length]
        hex = ' '.join(["%02x" % ord(x) for x in chars])
        if len(hex) > 24:
            hex = "%s %s" % (hex[:24], hex[24:])
        printable = ''.join(["%s" % FILTER[ord(x)] for x in chars])
        lines.append("%08x:  %-*s  |%s|\n" % (c, length*3, hex, printable))
    print(''.join(lines))
