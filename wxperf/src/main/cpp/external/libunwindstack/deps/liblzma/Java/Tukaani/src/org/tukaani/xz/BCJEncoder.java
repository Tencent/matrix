/*
 * BCJEncoder
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

class BCJEncoder extends BCJCoder implements FilterEncoder {
    private final BCJOptions options;
    private final long filterID;
    private final byte[] props;

    BCJEncoder(BCJOptions options, long filterID) {
        assert isBCJFilterID(filterID);
        int startOffset = options.getStartOffset();

        if (startOffset == 0) {
            props = new byte[0];
        } else {
            props = new byte[4];
            for (int i = 0; i < 4; ++i)
                props[i] = (byte)(startOffset >>> (i * 8));
        }

        this.filterID = filterID;
        this.options = (BCJOptions)options.clone();
    }

    public long getFilterID() {
        return filterID;
    }

    public byte[] getFilterProps() {
        return props;
    }

    public boolean supportsFlushing() {
        return false;
    }

    public FinishableOutputStream getOutputStream(FinishableOutputStream out) {
        return options.getOutputStream(out);
    }
}
