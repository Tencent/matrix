/*
 * RangeDecoderFromBuffer
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <http://7-zip.org/>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.rangecoder;

import java.io.DataInputStream;
import java.io.IOException;
import org.tukaani.xz.CorruptedInputException;

public final class RangeDecoderFromBuffer extends RangeDecoder {
    private static final int INIT_SIZE = 5;

    private final byte[] buf;
    private int pos = 0;
    private int end = 0;

    public RangeDecoderFromBuffer(int inputSizeMax) {
        buf = new byte[inputSizeMax - INIT_SIZE];
    }

    public void prepareInputBuffer(DataInputStream in, int len)
            throws IOException {
        if (len < INIT_SIZE)
            throw new CorruptedInputException();

        if (in.readUnsignedByte() != 0x00)
            throw new CorruptedInputException();

        code = in.readInt();
        range = 0xFFFFFFFF;

        pos = 0;
        end = len - INIT_SIZE;
        in.readFully(buf, 0, end);
    }

    public boolean isInBufferOK() {
        return pos <= end;
    }

    public boolean isFinished() {
        return pos == end && code == 0;
    }

    public void normalize() throws IOException {
        if ((range & TOP_MASK) == 0) {
            try {
                // If the input is corrupt, this might throw
                // ArrayIndexOutOfBoundsException.
                code = (code << SHIFT_BITS) | (buf[pos++] & 0xFF);
                range <<= SHIFT_BITS;
            } catch (ArrayIndexOutOfBoundsException e) {
                throw new CorruptedInputException();
            }
        }
    }
}
