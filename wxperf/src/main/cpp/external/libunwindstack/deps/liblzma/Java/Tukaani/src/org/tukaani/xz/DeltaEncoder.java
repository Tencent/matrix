/*
 * DeltaEncoder
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

class DeltaEncoder extends DeltaCoder implements FilterEncoder {
    private final DeltaOptions options;
    private final byte[] props = new byte[1];

    DeltaEncoder(DeltaOptions options) {
        props[0] = (byte)(options.getDistance() - 1);
        this.options = (DeltaOptions)options.clone();
    }

    public long getFilterID() {
        return FILTER_ID;
    }

    public byte[] getFilterProps() {
        return props;
    }

    public boolean supportsFlushing() {
        return true;
    }

    public FinishableOutputStream getOutputStream(FinishableOutputStream out) {
        return options.getOutputStream(out);
    }
}
