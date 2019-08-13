/*
 * Hash Chain match finder with 2-, 3-, and 4-byte hashing
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <http://7-zip.org/>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.lz;

final class HC4 extends LZEncoder {
    private final Hash234 hash;
    private final int[] chain;
    private final Matches matches;
    private final int depthLimit;

    private final int cyclicSize;
    private int cyclicPos = -1;
    private int lzPos;

    /**
     * Gets approximate memory usage of the match finder as kibibytes.
     */
    static int getMemoryUsage(int dictSize) {
        return Hash234.getMemoryUsage(dictSize) + dictSize / (1024 / 4) + 10;
    }

    /**
     * Creates a new LZEncoder with the HC4 match finder.
     * See <code>LZEncoder.getInstance</code> for parameter descriptions.
     */
    HC4(int dictSize, int beforeSizeMin, int readAheadMax,
            int niceLen, int matchLenMax, int depthLimit) {
        super(dictSize, beforeSizeMin, readAheadMax, niceLen, matchLenMax);

        hash = new Hash234(dictSize);

        // +1 because we need dictSize bytes of history + the current byte.
        cyclicSize = dictSize + 1;
        chain = new int[cyclicSize];
        lzPos = cyclicSize;

        // Substracting 1 because the shortest match that this match
        // finder can find is 2 bytes, so there's no need to reserve
        // space for one-byte matches.
        matches = new Matches(niceLen - 1);

        // Use a default depth limit if no other value was specified.
        // The default is just something based on experimentation;
        // it's nothing magic.
        this.depthLimit = (depthLimit > 0) ? depthLimit : 4 + niceLen / 4;
    }

    /**
     * Moves to the next byte, checks that there is enough available space,
     * and possibly normalizes the hash tables and the hash chain.
     *
     * @return      number of bytes available, including the current byte
     */
    private int movePos() {
        int avail = movePos(4, 4);

        if (avail != 0) {
            if (++lzPos == Integer.MAX_VALUE) {
                int normalizationOffset = Integer.MAX_VALUE - cyclicSize;
                hash.normalize(normalizationOffset);
                normalize(chain, normalizationOffset);
                lzPos -= normalizationOffset;
            }

            if (++cyclicPos == cyclicSize)
                cyclicPos = 0;
        }

        return avail;
    }

    public Matches getMatches() {
        matches.count = 0;
        int matchLenLimit = matchLenMax;
        int niceLenLimit = niceLen;
        int avail = movePos();

        if (avail < matchLenLimit) {
            if (avail == 0)
                return matches;

            matchLenLimit = avail;
            if (niceLenLimit > avail)
                niceLenLimit = avail;
        }

        hash.calcHashes(buf, readPos);
        int delta2 = lzPos - hash.getHash2Pos();
        int delta3 = lzPos - hash.getHash3Pos();
        int currentMatch = hash.getHash4Pos();
        hash.updateTables(lzPos);

        chain[cyclicPos] = currentMatch;

        int lenBest = 0;

        // See if the hash from the first two bytes found a match.
        // The hashing algorithm guarantees that if the first byte
        // matches, also the second byte does, so there's no need to
        // test the second byte.
        if (delta2 < cyclicSize && buf[readPos - delta2] == buf[readPos]) {
            lenBest = 2;
            matches.len[0] = 2;
            matches.dist[0] = delta2 - 1;
            matches.count = 1;
        }

        // See if the hash from the first three bytes found a match that
        // is different from the match possibly found by the two-byte hash.
        // Also here the hashing algorithm guarantees that if the first byte
        // matches, also the next two bytes do.
        if (delta2 != delta3 && delta3 < cyclicSize
                && buf[readPos - delta3] == buf[readPos]) {
            lenBest = 3;
            matches.dist[matches.count++] = delta3 - 1;
            delta2 = delta3;
        }

        // If a match was found, see how long it is.
        if (matches.count > 0) {
            while (lenBest < matchLenLimit && buf[readPos + lenBest - delta2]
                                              == buf[readPos + lenBest])
                ++lenBest;

            matches.len[matches.count - 1] = lenBest;

            // Return if it is long enough (niceLen or reached the end of
            // the dictionary).
            if (lenBest >= niceLenLimit)
                return matches;
        }

        // Long enough match wasn't found so easily. Look for better matches
        // from the hash chain.
        if (lenBest < 3)
            lenBest = 3;

        int depth = depthLimit;

        while (true) {
            int delta = lzPos - currentMatch;

            // Return if the search depth limit has been reached or
            // if the distance of the potential match exceeds the
            // dictionary size.
            if (depth-- == 0 || delta >= cyclicSize)
                return matches;

            currentMatch = chain[cyclicPos - delta
                                 + (delta > cyclicPos ? cyclicSize : 0)];

            // Test the first byte and the first new byte that would give us
            // a match that is at least one byte longer than lenBest. This
            // too short matches get quickly skipped.
            if (buf[readPos + lenBest - delta] == buf[readPos + lenBest]
                    && buf[readPos - delta] == buf[readPos]) {
                // Calculate the length of the match.
                int len = 0;
                while (++len < matchLenLimit)
                    if (buf[readPos + len - delta] != buf[readPos + len])
                        break;

                // Use the match if and only if it is better than the longest
                // match found so far.
                if (len > lenBest) {
                    lenBest = len;
                    matches.len[matches.count] = len;
                    matches.dist[matches.count] = delta - 1;
                    ++matches.count;

                    // Return if it is long enough (niceLen or reached the
                    // end of the dictionary).
                    if (len >= niceLenLimit)
                        return matches;
                }
            }
        }
    }

    public void skip(int len) {
        assert len >= 0;

        while (len-- > 0) {
            if (movePos() != 0) {
                // Update the hash chain and hash tables.
                hash.calcHashes(buf, readPos);
                chain[cyclicPos] = hash.getHash4Pos();
                hash.updateTables(lzPos);
            }
        }
    }
}
