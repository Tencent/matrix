package external.libunwindstack.deps.liblzma.Java.Tukaani.src;/*
 * XZDecDemo
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

import java.io.*;
import org.tukaani.xz.*;

/**
 * Decompresses .xz files to standard output. If no arguments are given,
 * reads from standard input.
 */
class XZDecDemo {
    public static void main(String[] args) {
        byte[] buf = new byte[8192];
        String name = null;

        try {
            if (args.length == 0) {
                name = "standard input";
                InputStream in = new XZInputStream(System.in);

                int size;
                while ((size = in.read(buf)) != -1)
                    System.out.write(buf, 0, size);

            } else {
                // Read from files given on the command line.
                for (int i = 0; i < args.length; ++i) {
                    name = args[i];
                    InputStream in = new FileInputStream(name);

                    try {
                        // Since XZInputStream does some buffering internally
                        // anyway, BufferedInputStream doesn't seem to be
                        // needed here to improve performance.
                        // in = new BufferedInputStream(in);
                        in = new XZInputStream(in);

                        int size;
                        while ((size = in.read(buf)) != -1)
                            System.out.write(buf, 0, size);

                    } finally {
                        // Close FileInputStream (directly or indirectly
                        // via XZInputStream, it doesn't matter).
                        in.close();
                    }
                }
            }
        } catch (FileNotFoundException e) {
            System.err.println("XZDecDemo: Cannot open " + name + ": "
                               + e.getMessage());
            System.exit(1);

        } catch (EOFException e) {
            System.err.println("XZDecDemo: Unexpected end of input on "
                               + name);
            System.exit(1);

        } catch (IOException e) {
            System.err.println("XZDecDemo: Error decompressing from "
                               + name + ": " + e.getMessage());
            System.exit(1);
        }
    }
}
