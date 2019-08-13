/*
 * EncoderUtil
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.common;

import java.io.OutputStream;
import java.io.IOException;
import java.util.zip.CRC32;

public class EncoderUtil extends Util {
    public static void writeCRC32(OutputStream out, byte[] buf)
            throws IOException {
        CRC32 crc32 = new CRC32();
        crc32.update(buf);
        long value = crc32.getValue();

        for (int i = 0; i < 4; ++i)
            out.write((byte)(value >>> (i * 8)));
    }

    public static void encodeVLI(OutputStream out, long num)
            throws IOException {
        while (num >= 0x80) {
            out.write((byte)(num | 0x80));
            num >>>= 7;
        }

        out.write((byte)num);
    }
}
