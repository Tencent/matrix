/*
 * BCJ filter for little endian ARM instructions
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <http://7-zip.org/>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.simple;

public final class ARM implements SimpleFilter {
    private final boolean isEncoder;
    private int pos;

    public ARM(boolean isEncoder, int startPos) {
        this.isEncoder = isEncoder;
        pos = startPos + 8;
    }

    public int code(byte[] buf, int off, int len) {
        int end = off + len - 4;
        int i;

        for (i = off; i <= end; i += 4) {
            if ((buf[i + 3] & 0xFF) == 0xEB) {
                int src = ((buf[i + 2] & 0xFF) << 16)
                          | ((buf[i + 1] & 0xFF) << 8)
                          | (buf[i] & 0xFF);
                src <<= 2;

                int dest;
                if (isEncoder)
                    dest = src + (pos + i - off);
                else
                    dest = src - (pos + i - off);

                dest >>>= 2;
                buf[i + 2] = (byte)(dest >>> 16);
                buf[i + 1] = (byte)(dest >>> 8);
                buf[i] = (byte)dest;
            }
        }

        i -= off;
        pos += i;
        return i;
    }
}
