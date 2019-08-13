/*
 * FinishableOutputStream
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

import java.io.OutputStream;
import java.io.IOException;

/**
 * Output stream that supports finishing without closing
 * the underlying stream.
 */
public abstract class FinishableOutputStream extends OutputStream {
    /**
     * Finish the stream without closing the underlying stream.
     * No more data may be written to the stream after finishing.
     * <p>
     * The <code>finish</code> method of <code>FinishableOutputStream</code>
     * does nothing. Subclasses should override it if they need finishing
     * support, which is the case, for example, with compressors.
     *
     * @throws      IOException
     */
    public void finish() throws IOException {};
}
