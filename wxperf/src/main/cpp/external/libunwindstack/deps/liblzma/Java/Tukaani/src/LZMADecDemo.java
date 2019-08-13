package external.libunwindstack.deps.liblzma.Java.Tukaani.src;/*
 * LZMADecDemo
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

import java.io.*;
import org.tukaani.xz.*;

/**
 * Decompresses .lzma files to standard output. If no arguments are given,
 * reads from standard input.
 *
 * NOTE: For most purposes, .lzma is a legacy format and usually you should
 * use .xz instead.
 */
class LZMADecDemo {
    public static void main(String[] args) {
        byte[] buf = new byte[8192];
        String name = null;

        try {
            if (args.length == 0) {
                name = "standard input";

                // No need to use BufferedInputStream with System.in which
                // seems to be fast with one-byte reads.
                InputStream in = new LZMAInputStream(System.in);

                int size;
                while ((size = in.read(buf)) != -1)
                    System.out.write(buf, 0, size);

            } else {
                // Read from files given on the command line.
                for (int i = 0; i < args.length; ++i) {
                    name = args[i];
                    InputStream in = new FileInputStream(name);

                    try {
                        // In contrast to other classes in org.tukaani.xz,
                        // LZMAInputStream doesn't do buffering internally
                        // and reads one byte at a time. BufferedInputStream
                        // gives a huge performance improvement here but even
                        // then it's slower than the other input streams from
                        // org.tukaani.xz.
                        in = new BufferedInputStream(in);
                        in = new LZMAInputStream(in);

                        int size;
                        while ((size = in.read(buf)) != -1)
                            System.out.write(buf, 0, size);

                    } finally {
                        // Close FileInputStream (directly or indirectly
                        // via LZMAInputStream, it doesn't matter).
                        in.close();
                    }
                }
            }
        } catch (FileNotFoundException e) {
            System.err.println("LZMADecDemo: Cannot open " + name + ": "
                               + e.getMessage());
            System.exit(1);

        } catch (EOFException e) {
            System.err.println("LZMADecDemo: Unexpected end of input on "
                               + name);
            System.exit(1);

        } catch (IOException e) {
            System.err.println("LZMADecDemo: Error decompressing from "
                               + name + ": " + e.getMessage());
            System.exit(1);
        }
    }
}
