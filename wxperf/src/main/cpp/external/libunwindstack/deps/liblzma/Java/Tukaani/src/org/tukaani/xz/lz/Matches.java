/*
 * Matches
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <http://7-zip.org/>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.lz;

public final class Matches {
    public final int[] len;
    public final int[] dist;
    public int count = 0;

    Matches(int countMax) {
        len = new int[countMax];
        dist = new int[countMax];
    }
}
