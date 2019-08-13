/*
 * DeltaDecoder
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.delta;

public class DeltaDecoder extends DeltaCoder {
    public DeltaDecoder(int distance) {
        super(distance);
    }

    public void decode(byte[] buf, int off, int len) {
        int end = off + len;
        for (int i = off; i < end; ++i) {
            buf[i] += history[(distance + pos) & DISTANCE_MASK];
            history[pos-- & DISTANCE_MASK] = buf[i];
        }
    }
}
