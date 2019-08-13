/*
 * CRC64
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.check;

public class CRC64 extends Check {
    private static final long poly = 0xC96C5795D7870F42L;
    private static final long[] crcTable = new long[256];

    private long crc = -1;

    static {
        for (int b = 0; b < crcTable.length; ++b) {
                long r = b;
                for (int i = 0; i < 8; ++i) {
                        if ((r & 1) == 1)
                                r = (r >>> 1) ^ poly;
                        else
                                r >>>= 1;
                }

                crcTable[b] = r;
        }
    }

    public CRC64() {
        size = 8;
        name = "CRC64";
    }

    public void update(byte[] buf, int off, int len) {
        int end = off + len;

        while (off < end)
            crc = crcTable[(buf[off++] ^ (int)crc) & 0xFF] ^ (crc >>> 8);
    }

    public byte[] finish() {
        long value = ~crc;
        crc = -1;

        byte[] buf = new byte[8];
        for (int i = 0; i < buf.length; ++i)
            buf[i] = (byte)(value >> (i * 8));

        return buf;
    }
}
