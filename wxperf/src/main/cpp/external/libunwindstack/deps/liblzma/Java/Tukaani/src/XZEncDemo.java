package external.libunwindstack.deps.liblzma.Java.Tukaani.src;/*
 * XZEncDemo
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

import java.io.*;
import org.tukaani.xz.*;

/**
 * Compresses a single file from standard input to standard ouput into
 * the .xz file format.
 * <p>
 * One optional argument is supported: LZMA2 preset level which is an integer
 * in the range [0, 9]. The default is 6.
 */
class XZEncDemo {
    public static void main(String[] args) throws Exception {
        LZMA2Options options = new LZMA2Options();

        if (args.length >= 1)
            options.setPreset(Integer.parseInt(args[0]));

        System.err.println("Encoder memory usage: "
                           + options.getEncoderMemoryUsage() + " KiB");
        System.err.println("Decoder memory usage: "
                           + options.getDecoderMemoryUsage() + " KiB");

        XZOutputStream out = new XZOutputStream(System.out, options);

        byte[] buf = new byte[8192];
        int size;
        while ((size = System.in.read(buf)) != -1)
            out.write(buf, 0, size);

        out.finish();
    }
}
