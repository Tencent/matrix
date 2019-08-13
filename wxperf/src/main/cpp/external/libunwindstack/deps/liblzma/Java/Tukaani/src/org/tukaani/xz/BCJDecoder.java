/*
 * BCJDecoder
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

import java.io.InputStream;
import org.tukaani.xz.simple.*;

class BCJDecoder extends BCJCoder implements FilterDecoder {
    private final long filterID;
    private final int startOffset;

    BCJDecoder(long filterID, byte[] props)
            throws UnsupportedOptionsException {
        assert isBCJFilterID(filterID);
        this.filterID = filterID;

        if (props.length == 0) {
            startOffset = 0;
        } else if (props.length == 4) {
            int n = 0;
            for (int i = 0; i < 4; ++i)
                n |= (props[i] & 0xFF) << (i * 8);

            startOffset = n;
        } else {
            throw new UnsupportedOptionsException(
                    "Unsupported BCJ filter properties");
        }
    }

    public int getMemoryUsage() {
        return SimpleInputStream.getMemoryUsage();
    }

    public InputStream getInputStream(InputStream in) {
        SimpleFilter simpleFilter = null;

        if (filterID == X86_FILTER_ID)
            simpleFilter = new X86(false, startOffset);
        else if (filterID == POWERPC_FILTER_ID)
            simpleFilter = new PowerPC(false, startOffset);
        else if (filterID == IA64_FILTER_ID)
            simpleFilter = new IA64(false, startOffset);
        else if (filterID == ARM_FILTER_ID)
            simpleFilter = new ARM(false, startOffset);
        else if (filterID == ARMTHUMB_FILTER_ID)
            simpleFilter = new ARMThumb(false, startOffset);
        else if (filterID == SPARC_FILTER_ID)
            simpleFilter = new SPARC(false, startOffset);
        else
            assert false;

        return new SimpleInputStream(in, simpleFilter);
    }
}
