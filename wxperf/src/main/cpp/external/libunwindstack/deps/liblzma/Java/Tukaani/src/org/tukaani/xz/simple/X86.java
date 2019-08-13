/*
 * BCJ filter for x86 instructions
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <http://7-zip.org/>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.simple;

public final class X86 implements SimpleFilter {
    private static final boolean[] MASK_TO_ALLOWED_STATUS
            = {true, true, true, false, true, false, false, false};

    private static final int[] MASK_TO_BIT_NUMBER = {0, 1, 2, 2, 3, 3, 3, 3};

    private final boolean isEncoder;
    private int pos;
    private int prevMask = 0;

    private static boolean test86MSByte(byte b) {
        int i = b & 0xFF;
        return i == 0x00 || i == 0xFF;
    }

    public X86(boolean isEncoder, int startPos) {
        this.isEncoder = isEncoder;
        pos = startPos + 5;
    }

    public int code(byte[] buf, int off, int len) {
        int prevPos = off - 1;
        int end = off + len - 5;
        int i;

        for (i = off; i <= end; ++i) {
            if ((buf[i] & 0xFE) != 0xE8)
                continue;

            prevPos = i - prevPos;
            if ((prevPos & ~3) != 0) { // (unsigned)prevPos > 3
                prevMask = 0;
            } else {
                prevMask = (prevMask << (prevPos - 1)) & 7;
                if (prevMask != 0) {
                    if (!MASK_TO_ALLOWED_STATUS[prevMask] || test86MSByte(
                            buf[i + 4 - MASK_TO_BIT_NUMBER[prevMask]])) {
                        prevPos = i;
                        prevMask = (prevMask << 1) | 1;
                        continue;
                    }
                }
            }

            prevPos = i;

            if (test86MSByte(buf[i + 4])) {
                int src = (buf[i + 1] & 0xFF)
                          | ((buf[i + 2] & 0xFF) << 8)
                          | ((buf[i + 3] & 0xFF) << 16)
                          | ((buf[i + 4] & 0xFF) << 24);
                int dest;
                while (true) {
                    if (isEncoder)
                        dest = src + (pos + i - off);
                    else
                        dest = src - (pos + i - off);

                    if (prevMask == 0)
                        break;

                    int index = MASK_TO_BIT_NUMBER[prevMask] * 8;
                    if (!test86MSByte((byte)(dest >>> (24 - index))))
                        break;

                    src = dest ^ ((1 << (32 - index)) - 1);
                }

                buf[i + 1] = (byte)dest;
                buf[i + 2] = (byte)(dest >>> 8);
                buf[i + 3] = (byte)(dest >>> 16);
                buf[i + 4] = (byte)(~(((dest >>> 24) & 1) - 1));
                i += 4;
            } else {
                prevMask = (prevMask << 1) | 1;
            }
        }

        prevPos = i - prevPos;
        prevMask = ((prevPos & ~3) != 0) ? 0 : prevMask << (prevPos - 1);

        i -= off;
        pos += i;
        return i;
    }
}
