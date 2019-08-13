/*
 * BCJ filter for SPARC instructions
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <http://7-zip.org/>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.simple;

public final class SPARC implements SimpleFilter {
    private final boolean isEncoder;
    private int pos;

    public SPARC(boolean isEncoder, int startPos) {
        this.isEncoder = isEncoder;
        pos = startPos;
    }

    public int code(byte[] buf, int off, int len) {
        int end = off + len - 4;
        int i;

        for (i = off; i <= end; i += 4) {
            if ((buf[i] == 0x40 && (buf[i + 1] & 0xC0) == 0x00)
                    || (buf[i] == 0x7F && (buf[i + 1] & 0xC0) == 0xC0)) {
                int src = ((buf[i] & 0xFF) << 24)
                          | ((buf[i + 1] & 0xFF) << 16)
                          | ((buf[i + 2] & 0xFF) << 8)
                          | (buf[i + 3] & 0xFF);
                src <<= 2;

                int dest;
                if (isEncoder)
                    dest = src + (pos + i - off);
                else
                    dest = src - (pos + i - off);

                dest >>>= 2;
                dest = (((0 - ((dest >>> 22) & 1)) << 22) & 0x3FFFFFFF)
                       | (dest & 0x3FFFFF) | 0x40000000;

                buf[i] = (byte)(dest >>> 24);
                buf[i + 1] = (byte)(dest >>> 16);
                buf[i + 2] = (byte)(dest >>> 8);
                buf[i + 3] = (byte)dest;
            }
        }

        i -= off;
        pos += i;
        return i;
    }
}
