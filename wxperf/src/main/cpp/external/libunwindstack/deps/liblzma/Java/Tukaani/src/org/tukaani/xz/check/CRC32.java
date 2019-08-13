/*
 * CRC32
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.check;

public class CRC32 extends Check {
    private final java.util.zip.CRC32 state = new java.util.zip.CRC32();

    public CRC32() {
        size = 4;
        name = "CRC32";
    }

    public void update(byte[] buf, int off, int len) {
        state.update(buf, off, len);
    }

    public byte[] finish() {
        long value = state.getValue();
        byte[] buf = { (byte)(value),
                       (byte)(value >>> 8),
                       (byte)(value >>> 16),
                       (byte)(value >>> 24) };
        state.reset();
        return buf;
    }
}
