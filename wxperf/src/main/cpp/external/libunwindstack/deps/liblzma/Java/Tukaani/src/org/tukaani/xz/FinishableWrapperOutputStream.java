/*
 * FinishableWrapperOutputStream
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
 * Wraps an output stream to a finishable output stream for use with
 * raw encoders. This is not needed for XZ compression and thus most
 * people will never need this.
 */
public class FinishableWrapperOutputStream extends FinishableOutputStream {
    /**
     * The {@link OutputStream OutputStream} that has been
     * wrapped into a FinishableWrapperOutputStream.
     */
    protected OutputStream out;

    /**
     * Creates a new output stream which support finishing.
     * The <code>finish()</code> method will do nothing.
     */
    public FinishableWrapperOutputStream(OutputStream out) {
        this.out = out;
    }

    /**
     * Calls {@link OutputStream#write(int) out.write(b)}.
     */
    public void write(int b) throws IOException {
        out.write(b);
    }

    /**
     * Calls {@link OutputStream#write(byte[]) out.write(buf)}.
     */
    public void write(byte[] buf) throws IOException {
        out.write(buf);
    }

    /**
     * Calls {@link OutputStream#write(byte[],int,int)
                    out.write(buf, off, len)}.
     */
    public void write(byte[] buf, int off, int len) throws IOException {
        out.write(buf, off, len);
    }

    /**
     * Calls {@link OutputStream#flush() out.flush()}.
     */
    public void flush() throws IOException {
        out.flush();
    }

    /**
     * Calls {@link OutputStream#close() out.close()}.
     */
    public void close() throws IOException {
        out.close();
    }
}
