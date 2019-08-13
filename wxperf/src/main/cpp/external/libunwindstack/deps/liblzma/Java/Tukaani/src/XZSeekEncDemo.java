package external.libunwindstack.deps.liblzma.Java.Tukaani.src;/*
 * XZSeekEncDemo
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
 * a random-accessible .xz file.
 * <p>
 * Arguments: [preset [block size]]
 * <p>
 * Preset is an LZMA2 preset level which is an integer in the range [0, 9].
 * The default is 6.
 * <p>
 * Block size specifies the amount of uncompressed data to store per
 * XZ Block. The default is 1 MiB (1048576 bytes). Bigger means better
 * compression ratio. Smaller means faster random access.
 */
class XZSeekEncDemo {
    public static void main(String[] args) throws Exception {
        LZMA2Options options = new LZMA2Options();

        if (args.length >= 1)
            options.setPreset(Integer.parseInt(args[0]));

        int blockSize = 1024 * 1024;
        if (args.length >= 2)
            blockSize = Integer.parseInt(args[1]);

        options.setDictSize(Math.min(options.getDictSize(),
                                     Math.max(LZMA2Options.DICT_SIZE_MIN,
                                              blockSize)));

        System.err.println("Encoder memory usage: "
                           + options.getEncoderMemoryUsage() + " KiB");
        System.err.println("Decoder memory usage: "
                           + options.getDecoderMemoryUsage() + " KiB");
        System.err.println("Block size:           " + blockSize + " B");

        XZOutputStream out = new XZOutputStream(System.out, options);

        byte[] buf = new byte[8192];
        int left = blockSize;

        while (true) {
            int size = System.in.read(buf, 0, Math.min(buf.length, left));
            if (size == -1)
                break;

            out.write(buf, 0, size);
            left -= size;

            if (left == 0) {
                out.endBlock();
                left = blockSize;
            }
        }

        out.finish();
    }
}
