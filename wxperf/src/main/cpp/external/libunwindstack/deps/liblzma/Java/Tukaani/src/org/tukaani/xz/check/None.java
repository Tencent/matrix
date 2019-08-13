/*
 * None
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.check;

public class None extends Check {
    public None() {
        size = 0;
        name = "None";
    }

    public void update(byte[] buf, int off, int len) {}

    public byte[] finish() {
        byte[] empty = new byte[0];
        return empty;
    }
}
