/*
 * DeltaDecoder
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

import java.io.InputStream;

class DeltaDecoder extends DeltaCoder implements FilterDecoder {
    private final int distance;

    DeltaDecoder(byte[] props) throws UnsupportedOptionsException {
        if (props.length != 1)
            throw new UnsupportedOptionsException(
                    "Unsupported Delta filter properties");

        distance = (props[0] & 0xFF) + 1;
    }

    public int getMemoryUsage() {
        return 1;
    }

    public InputStream getInputStream(InputStream in) {
        return new DeltaInputStream(in, distance);
    }
}
