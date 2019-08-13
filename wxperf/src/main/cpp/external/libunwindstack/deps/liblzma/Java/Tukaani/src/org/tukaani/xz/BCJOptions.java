/*
 * BCJOptions
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

abstract class BCJOptions extends FilterOptions {
    private final int alignment;
    int startOffset = 0;

    BCJOptions(int alignment) {
        this.alignment = alignment;
    }

    /**
     * Sets the start offset for the address conversions.
     * Normally this is useless so you shouldn't use this function.
     * The default value is <code>0</code>.
     */
    public void setStartOffset(int startOffset)
            throws UnsupportedOptionsException {
        if ((startOffset & (alignment - 1)) != 0)
            throw new UnsupportedOptionsException(
                    "Start offset must be a multiple of " + alignment);

        this.startOffset = startOffset;
    }

    /**
     * Gets the start offset.
     */
    public int getStartOffset() {
        return startOffset;
    }

    public int getEncoderMemoryUsage() {
        return SimpleOutputStream.getMemoryUsage();
    }

    public int getDecoderMemoryUsage() {
        return SimpleInputStream.getMemoryUsage();
    }

    public Object clone() {
        try {
            return super.clone();
        } catch (CloneNotSupportedException e) {
            assert false;
            throw new RuntimeException();
        }
    }
}
