/*
 * BCJ filter for big endian PowerPC instructions
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <http://7-zip.org/>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.simple;

public final class PowerPC implements SimpleFilter {
    private final boolean isEncoder;
    private int pos;

    public PowerPC(boolean isEncoder, int startPos) {
        this.isEncoder = isEncoder;
        pos = startPos;
    }

    public int code(byte[] buf, int off, int len) {
        int end = off + len - 4;
        int i;

        for (i = off; i <= end; i += 4) {
            if ((buf[i] & 0xFC) == 0x48 && (buf[i + 3] & 0x03) == 0x01) {
                int src = ((buf[i] & 0x03) << 24)
                          | ((buf[i + 1] & 0xFF) << 16)
                          | ((buf[i + 2] & 0xFF) << 8)
                          | (buf[i + 3] & 0xFC);

                int dest;
                if (isEncoder)
                    dest = src + (pos + i - off);
                else
                    dest = src - (pos + i - off);

                buf[i] = (byte)(0x48 | ((dest >>> 24) & 0x03));
                buf[i + 1] = (byte)(dest >>> 16);
                buf[i + 2] = (byte)(dest >>> 8);
                buf[i + 3] = (byte)((buf[i + 3] & 0x03) | dest);
            }
        }

        i -= off;
        pos += i;
        return i;
    }
}
