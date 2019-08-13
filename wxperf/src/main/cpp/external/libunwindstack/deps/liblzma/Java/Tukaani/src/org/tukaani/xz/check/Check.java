/*
 * Check
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.check;

import org.tukaani.xz.XZ;
import org.tukaani.xz.UnsupportedOptionsException;

public abstract class Check {
    int size;
    String name;

    public abstract void update(byte[] buf, int off, int len);
    public abstract byte[] finish();

    public void update(byte[] buf) {
        update(buf, 0, buf.length);
    }

    public int getSize() {
        return size;
    }

    public String getName() {
        return name;
    }

    public static Check getInstance(int checkType)
            throws UnsupportedOptionsException {
        switch (checkType) {
            case XZ.CHECK_NONE:
                return new None();

            case XZ.CHECK_CRC32:
                return new CRC32();

            case XZ.CHECK_CRC64:
                return new CRC64();

            case XZ.CHECK_SHA256:
                try {
                    return new SHA256();
                } catch (java.security.NoSuchAlgorithmException e) {}

                break;
        }

        throw new UnsupportedOptionsException(
                "Unsupported Check ID " + checkType);
    }
}
