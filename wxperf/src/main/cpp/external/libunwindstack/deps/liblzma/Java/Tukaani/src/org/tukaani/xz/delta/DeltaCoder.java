/*
 * DeltaCoder
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.delta;

abstract class DeltaCoder {
    static final int DISTANCE_MIN = 1;
    static final int DISTANCE_MAX = 256;
    static final int DISTANCE_MASK = DISTANCE_MAX - 1;

    final int distance;
    final byte[] history = new byte[DISTANCE_MAX];
    int pos = 0;

    DeltaCoder(int distance) {
        if (distance < DISTANCE_MIN || distance > DISTANCE_MAX)
            throw new IllegalArgumentException();

        this.distance = distance;
    }
}
