/*
 * CRC32Hash
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <http://7-zip.org/>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.lz;

/**
 * Provides a CRC32 table using the polynomial from IEEE 802.3.
 */
class CRC32Hash {
    private static final int CRC32_POLY = 0xEDB88320;

    static final int[] crcTable = new int[256];

    static {
        for (int i = 0; i < 256; ++i) {
            int r = i;

            for (int j = 0; j < 8; ++j) {
                if ((r & 1) != 0)
                    r = (r >>> 1) ^ CRC32_POLY;
                else
                    r >>>= 1;
            }

            crcTable[i] = r;
        }
    }
}
