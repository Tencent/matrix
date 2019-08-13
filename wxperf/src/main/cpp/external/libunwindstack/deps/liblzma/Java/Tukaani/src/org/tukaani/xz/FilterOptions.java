/*
 * FilterOptions
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <http://7-zip.org/>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

import java.io.InputStream;
import java.io.IOException;

/**
 * Base class for filter-specific options classes.
 */
public abstract class FilterOptions implements Cloneable {
    /**
     * Gets how much memory the encoder will need with
     * the given filter chain. This function simply calls
     * <code>getEncoderMemoryUsage()</code> for every filter
     * in the array and returns the sum of the returned values.
     */
    public static int getEncoderMemoryUsage(FilterOptions[] options) {
        int m = 0;

        for (int i = 0; i < options.length; ++i)
            m += options[i].getEncoderMemoryUsage();

        return m;
    }

    /**
     * Gets how much memory the decoder will need with
     * the given filter chain. This function simply calls
     * <code>getDecoderMemoryUsage()</code> for every filter
     * in the array and returns the sum of the returned values.
     */
    public static int getDecoderMemoryUsage(FilterOptions[] options) {
        int m = 0;

        for (int i = 0; i < options.length; ++i)
            m += options[i].getDecoderMemoryUsage();

        return m;
    }

    /**
     * Gets how much memory the encoder will need with these options.
     */
    public abstract int getEncoderMemoryUsage();

    /**
     * Gets a raw (no XZ headers) encoder output stream using these options.
     * Raw streams are an advanced feature. In most cases you want to store
     * the compressed data in the .xz container format instead of using
     * a raw stream. To use this filter in a .xz file, pass this object
     * to XZOutputStream.
     */
    public abstract FinishableOutputStream getOutputStream(
            FinishableOutputStream out);

    /**
     * Gets how much memory the decoder will need to decompress the data
     * that was encoded with these options.
     */
    public abstract int getDecoderMemoryUsage();

    /**
     * Gets a raw (no XZ headers) decoder input stream using these options.
     */
    public abstract InputStream getInputStream(InputStream in)
            throws IOException;

    abstract FilterEncoder getFilterEncoder();

    FilterOptions() {}
}
