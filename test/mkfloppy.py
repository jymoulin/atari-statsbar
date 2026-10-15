#!/usr/bin/env python3
"""Fabrique une image disquette Atari 720 Ko (FAT12) contenant des fichiers.

Sert a tester sans l'emulation GEMDOS de Hatari, qui court-circuite le BIOS
et ne permet donc pas de valider ce qui touche a hdv_rw. Sert aussi a
demarrer FreeMiNT, dont le pilote FAT lit des secteurs et ne voit donc pas
un disque GEMDOS emule.

   python3 test/mkfloppy.py [--hd] sortie.st fichier [fichier...]
   python3 test/mkfloppy.py [--hd] sortie.st source=AUTO/MINT.PRG ...

--hd fabrique une image 1,44 Mo (FreeMiNT + XaAES ne tiennent pas en
720 Ko).

Un argument "source=chemin" place le fichier dans l'image a ce chemin
(separateur / ou \\), en creant les repertoires. Sans "=", le fichier va a
la racine sous son propre nom. Un repertoire tient dans un cluster, soit
30 entrees, ce qui suffit ici.
"""
import struct, sys, os

SEC, SPC, RES, NFAT, ROOTENT, TOTSEC, SPF, SPT, HEADS, MEDIA = \
    512, 2, 1, 2, 112, 1440, 5, 9, 2, 0xF9
if len(sys.argv) > 1 and sys.argv[1] == '--hd':
    del sys.argv[1]
    SPC, ROOTENT, TOTSEC, SPF, SPT, MEDIA = 1, 224, 2880, 9, 18, 0xF0
CLU = SPC * SEC


def name83(name):
    base, _, ext = name.upper().partition('.')
    return (base[:8].ljust(8) + ext[:3].ljust(3)).encode('ascii')


def main():
    out, args = sys.argv[1], sys.argv[2:]
    img = bytearray(b'\0' * (TOTSEC * SEC))

    # secteur de boot (BPB en little-endian, comme le DOS)
    img[0:3] = b'\xe9\x00\x00'
    img[3:11] = b'STATSBAR'
    struct.pack_into('<H', img, 0x0B, SEC)
    img[0x0D] = SPC
    struct.pack_into('<H', img, 0x0E, RES)
    img[0x10] = NFAT
    struct.pack_into('<H', img, 0x11, ROOTENT)
    struct.pack_into('<H', img, 0x13, TOTSEC)
    img[0x15] = MEDIA
    struct.pack_into('<H', img, 0x16, SPF)
    struct.pack_into('<H', img, 0x18, SPT)
    struct.pack_into('<H', img, 0x1A, HEADS)

    fat = bytearray(b'\0' * (SPF * SEC))
    fat[0:3] = bytes([MEDIA, 0xFF, 0xFF])

    def set_fat(n, val):
        off = (n * 3) // 2
        if n % 2 == 0:
            fat[off] = val & 0xFF
            fat[off+1] = (fat[off+1] & 0xF0) | ((val >> 8) & 0x0F)
        else:
            fat[off] = (fat[off] & 0x0F) | ((val << 4) & 0xF0)
            fat[off+1] = (val >> 4) & 0xFF

    root_sec = RES + NFAT * SPF
    root_sec_cnt = (ROOTENT * 32 + SEC - 1) // SEC
    data_sec = root_sec + root_sec_cnt

    def cl_off(c):
        return (data_sec + (c - 2) * SPC) * SEC

    root = bytearray(b'\0' * (root_sec_cnt * SEC))
    state = {'cluster': 2}

    # repertoires : chemin -> [tampon d'entrees, cluster (0 = racine), n]
    dirs = {'': [root, 0, 0]}

    def add_entry(dpath, entry):
        d = dirs[dpath]
        off = d[2] * 32
        if off + 32 > len(d[0]):
            sys.exit("repertoire plein : %s" % (dpath or '\\'))
        d[0][off:off+32] = entry
        d[2] += 1

    def dirent(name11, attr, first, size):
        e = bytearray(32)
        e[0:11] = name11
        e[11] = attr
        struct.pack_into('<H', e, 22, 0)        # heure
        struct.pack_into('<H', e, 24, 0x2F21)   # date
        struct.pack_into('<H', e, 26, first)
        struct.pack_into('<I', e, 28, size)
        return e

    def make_dir(path):
        if path in dirs:
            return
        parent, _, leaf = path.rpartition('/')
        make_dir(parent)
        c = state['cluster']
        state['cluster'] += 1
        set_fat(c, 0xFFF)
        buf = bytearray(CLU)
        dirs[path] = [buf, c, 0]
        add_entry(path, dirent(b'.          ', 0x10, c, 0))
        add_entry(path, dirent(b'..         ', 0x10, dirs[parent][1], 0))
        add_entry(parent, dirent(name83(leaf), 0x10, c, 0))

    for arg in args:
        src, sep, dest = arg.partition('=')
        dest = dest.replace('\\', '/').strip('/').upper() if sep else \
            os.path.basename(src).upper()
        dpath, _, leaf = dest.rpartition('/')
        make_dir(dpath)

        data = open(src, 'rb').read()
        ncl = max(1, (len(data) + CLU - 1) // CLU)
        first = state['cluster']
        for k in range(ncl):
            c = state['cluster']
            set_fat(c, 0xFFF if k == ncl-1 else c+1)
            chunk = data[k*CLU:(k+1)*CLU]
            img[cl_off(c):cl_off(c)+len(chunk)] = chunk
            state['cluster'] += 1
        add_entry(dpath, dirent(name83(leaf), 0x20, first, len(data)))

    for path, (buf, c, n) in dirs.items():
        if path:
            img[cl_off(c):cl_off(c)+CLU] = buf

    img[RES*SEC:(RES+SPF)*SEC] = fat
    img[(RES+SPF)*SEC:(RES+2*SPF)*SEC] = fat
    img[root_sec*SEC:(root_sec+root_sec_cnt)*SEC] = root

    used = state['cluster'] - 2
    total = (TOTSEC - data_sec) // SPC
    if used > total:
        sys.exit("image trop petite : %d clusters pour %d" % (used, total))
    open(out, 'wb').write(img)
    print("%s : %d fichier(s), %d clusters sur %d" % (out, len(args), used, total))


main()
