/**
 * XZ data compression support.
 *
 * <h4>Introduction</h4>
 * <p>
 * This aims to be a complete implementation of XZ data compression
 * in pure Java. Features:
 * <ul>
 * <li>Full support for the .xz file format specification version 1.0.4</li>
 * <li>Single-threaded streamed compression and decompression</li>
 * <li>Single-threaded decompression with limited random access support</li>
 * <li>Raw streams (no .xz headers) for advanced users, including LZMA2
 *     with preset dictionary</li>
 * </ul>
 * <p>
 * Threading is planned but it is unknown when it will be implemented.
 * <p>
 * For the latest source code, see the
 * <a href="http://tukaani.org/xz/java.html">home page of XZ for Java</a>.
 *
 * <h4>Getting started</h4>
 * <p>
 * Start by reading the documentation of {@link org.tukaani.xz.XZOutputStream}
 * and {@link org.tukaani.xz.XZInputStream}.
 * If you use XZ inside another file format or protocol,
 * see also {@link org.tukaani.xz.SingleXZInputStream}.
 *
 * <h4>Licensing</h4>
 * <p>
 * XZ for Java has been put into the public domain, thus you can do
 * whatever you want with it. All the files in the package have been
 * written by Lasse Collin and/or Igor Pavlov.
 * <p>
 * This software is provided "as is", without any warranty.
 */
package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;
