/*
 * RangeDecoder
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

public abstract class RangeDecoder extends RangeCoder {
    int range = 0;
    int code = 0;

    public abstract void normalize() throws IOException;

    public int decodeBit(short[] probs, int index) throws IOException {
        normalize();

        int prob = probs[index];
        int bound = (range >>> BIT_MODEL_TOTAL_BITS) * prob;
        int bit;

        // Compare code and bound as if they were unsigned 32-bit integers.
        if ((code ^ 0x80000000) < (bound ^ 0x80000000)) {
            range = bound;
            probs[index] = (short)(
                    prob + ((BIT_MODEL_TOTAL - prob) >>> MOVE_BITS));
            bit = 0;
        } else {
            range -= bound;
            code -= bound;
            probs[index] = (short)(prob - (prob >>> MOVE_BITS));
            bit = 1;
        }

        return bit;
    }

    public int decodeBitTree(short[] probs) throws IOException {
        int symbol = 1;

        do {
            symbol = (symbol << 1) | decodeBit(probs, symbol);
        } while (symbol < probs.length);

        return symbol - probs.length;
    }

    public int decodeReverseBitTree(short[] probs) throws IOException {
        int symbol = 1;
        int i = 0;
        int result = 0;

        do {
            int bit = decodeBit(probs, symbol);
            symbol = (symbol << 1) | bit;
            result |= bit << i++;
        } while (symbol < probs.length);

        return result;
    }

    public int decodeDirectBits(int count) throws IOException {
        int result = 0;

        do {
            normalize();

            range >>>= 1;
            int t = (code - range) >>> 31;
            code -= range & (t - 1);
            result = (result << 1) | (1 - t);
        } while (--count != 0);

        return result;
    }
}
