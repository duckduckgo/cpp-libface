"""
face: Fastest Auto-Complete in the East
An efficient auto-complete library suitable for large volumes of data.


Copyright (c) 2010, Dhruv Matani
dhruvbird@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
"""


import math
import heapq


def lowerBound(seq, elem, lessThan):
    """
    Find the first position(index) in sequence 'seq' before
    which elem can be inserted. Use lessThan as the comparator.

    Complexity: O(log n)
    n -> len(seq)
    """
    f = 0
    l = len(seq)
    ti = f
    while f < l:
        m = (f+l) / 2

        if lessThan(seq[m], elem, 1):
            # Lower Bound lies in the right half
            ti = m
            f = m + 1
        else:
            l = m

    return ti


def upperBound(seq, elem, lessThan):
    """
    Find the last position(index) in sequence 'seq' before
    which elem can be inserted. Use lessThan as the comparator.

    Complexity: O(log n)
    n -> len(seq)
    """
    f = 0
    l = len(seq)
    ti = l
    while f < l:
        m = (f+l) / 2

        if lessThan(elem, seq[m], 0):
            l = m
        else:
            ti = m
            f = m + 1

    return ti


def merge(seq1, seq2, lessThan):
    """
    Merge the 2 sequences seq1 and seq2 using lessThan to compare
    elements from the 2 sequences. Return the merged list.

    Complexity: O(m + n)
    m -> len(seq1)
    n -> len(seq2)
    """
    ret = []
    i = 0
    j = 0
    l1 = len(seq1)
    l2 = len(seq2)
    while i < l1 and j < l2:
        if lessThan(seq1[i], seq2[j]):
            ret.append(seq1[i])
            i += 1
        else:
            ret.append(seq2[j])
            j += 1

    if i < l1:
        ret[len(ret):] = seq1[i:]

    if j < l2:
        ret[len(ret):] = seq2[j:]

    return ret


def deleteIndexes(seq, indexes):
    """
    Deletes entries at all entries at all indexes in 'indexes' in array 'seq'.
    This procedure modifies the sequence 'seq' in-place. Returns: nothing.
    """
    i1 = 0
    i2 = 0
    lseq = len(seq)
    j = 0
    while j < len(indexes) and i2 < lseq:
        if indexes[j] == i2:
            i2 += 1
            j  += 1
        elif i1 != i2:
            seq[i1] = seq[i2]
            i1 += 1
            i2 += 1
        else:
            i1 += 1
            i2 += 1

    if i1 != i2:
        rem = len(seq) - i2
        seq[i1:] = seq[i2:]
        del seq[i1 + rem:]


class SegmentTree(object):
    def __init__(self):
        self.impl = []
        self.size = 0

    def assign(self, seq):
        """
        Assign the sequence 'seq' to the segment tree.

        Complexity: O(n)
        n -> len(seq)
        """
        n = int(math.pow(2, math.ceil(math.log(len(seq), 2)) + 1))
        self.impl = [ (0,0) ] * n
        self.size = len(seq)
        if self.size > 0:
            self._assign(seq, 0, len(seq) - 1, 0)

    def _assign(self, seq, sb, se, i):
        if sb == se:
            self.impl[i] = (seq[sb], sb)
        else:
            m = (sb + se) / 2
            l = self._assign(seq, sb,  m,  i*2+1)
            r = self._assign(seq, m+1, se, i*2+2)
            if l[0] > r[0]:
                self.impl[i] = l
            else:
                self.impl[i] = r

        return self.impl[i]



    def query(self, f, l):
        """
        Query the segment tree for the largest value in the range [f, l].
        Returns a tuple (max, index) which has the maximum value as well as
        the index where the maximum occurs.

        Complexity: O(log n)
        n -> Number of elements in the segment tree
        """
        b = 0
        e = self.size - 1
        if self.size == 0 or f > e or l < b:
            return None

        return self._query(f, l, b, e, 0)

    def _query(self, f, l, b, e, i):
        # if b == e:
        #     return self.impl[i]

        if f==b and l==e:
            return self.impl[i]

        m = (b + e) / 2
        v1 = (-1, 0)
        v2 = (-1, 0)

        if f <= m:
            v1 = self._query(f, min(l, m), b, m, i*2+1)

        if l > m:
            v2 = self._query(max(f, m+1), l, m+1, e, i*2+2)

        if v1[0] > v2[0]:
            return v1
        else:
            return v2





class Entry(object):
    """
    Holds a single entry, which is a phrase (string) along with a score (integer)
    """
    __slots__ = ( "phrase", "score" )
    def __init__(self, phrase, score):
        self.phrase = phrase
        self.score  = score

    def __repr__(self):
        return "Phrase: %s, Score: %d" % (self.phrase, self.score)

def entry_less_than_phrase(lhs, rhs):
    return lhs.phrase < rhs.phrase

def entry_less_than_score(lhs, rhs):
    return lhs.score < rhs.score

def entry_lt(lhs, rhs, elemIndex):
    if elemIndex is 0:
        return lhs.phrase < rhs.phrase[0:len(lhs.phrase)]
    else:
        return lhs.phrase[0:len(rhs.phrase)] < rhs.phrase

def entry_lteq(lhs, rhs):
    return lhs.phrase <= rhs.phrase

def entry_gteq(lhs, rhs):
    return lhs.phrase >= rhs.phrase or rhs.phrase.find(lhs.phrase) == 0


class Range(object):
    def __init__(self, begin, end, score, scoreIndex):
        """
        Represents a range [begin, end). score is the maximum score for
        any entry in that range. scoreIndex is the index in the list of
        phrases where the entry with the maximum score occurs.
        """
        self.begin = begin
        self.end   = end
        self.score = score
        self.scoreIndex = scoreIndex

    def __lt__(self, rhs):
        return self.score > rhs.score

    def __le__(self, rhs):
        return self.score >= rhs.score

    def __repr__(self):
        return "(%d, %d, %d, %d)" % (self.begin, self.end,
                                     self.score, self.scoreIndex)

# TODO: Use prefix compression for strings that share the same prefix
class AutoComplete(object):
    NAIVE_ALGO_THRESHOLD_MULTIPLIER = 4

    def __init__(self):
        self.phrases   = []
        self.segTree   = SegmentTree()
        self.iBuffer   = []
        self.delBuffer = []

    def insert(self, entry):
        """
        Insert the entry 'entry' into the AutoComplete list. The changes come
        into effect ONLY after a commit() is peformed. (See Below)
        """
        self.iBuffer.append(entry)

    def remove(self, phrase):
        """
        Remove the entry with phrase 'phrase' from the AutoComplete list.
        The changes come into effect ONLY after a commit() is peformed.
        (See Below). You can NOT remove inserted but yet uncomitted entries.
        """

        self.delBuffer.append(phrase)

    def rollback(self):
        """
        Discard any changes made since the last commit()
        """
        self.iBuffer   = []
        self.delBuffer = []

    def commit(self):
        """
        Commit changes (if any) to the internal data stucture. This is an expensive
        operation and should be used after making a lot of changes to the data or
        unless absolutely necessary.
        """
        touched = False
        delIndexes = []

        for p in self.delBuffer:
            e = Entry(p, 0)
            pos = lowerBound(self.phrases, e, entry_less_than_phrase)
            if self.phrases[pos].phrase == p:
                delIndexes.append(pos)
                touched = True

        delIndexes.sort()
        deleteIndexes(self.phrases, delIndexes)

        if len(self.iBuffer) > 0:
            self.iBuffer.sort(key=lambda x: x.phrase)
            touched = True

        if touched:
            if len(self.phrases) == 0:
                self.phrases = self.iBuffer
            else:
                self.phrases = merge(self.phrases, self.iBuffer, entry_less_than_phrase)

            self.segTree.assign(map(lambda x: x.score, self.phrases))

        self.rollback()


    def query(self, prefix, k):
        """
        Return at most 'k' phrases in the dictionary such that 'prefix' is a prefix
        of every returned phrase. Additionally return the phrases in non-increasing
        order of score.

        Complexity: O(k log n)
        n -> Number of phrases in the dictionary
        """
        pos1 = lowerBound(self.phrases, Entry(prefix, 0), entry_lt)
        pos2 = upperBound(self.phrases, Entry(prefix, 0), entry_lt)
 
        # print "pos1: %d, pos2: %d" % (pos1, pos2)

        if pos2 - pos1 <= AutoComplete.NAIVE_ALGO_THRESHOLD_MULTIPLIER * k:
            tmp = self.phrases[pos1:pos2]
            tmp.sort(key=lambda x: x.score, reverse=True)
            tmp = tmp[:k]
            return tmp

        ret = []
        if pos1 == pos2:
            return ret

        q = self.segTree.query(pos1, pos2 - 1)

        h = [ Range(pos1, pos2, q[0], q[1]) ]
        while len(ret) < k and len(h) != 0:
            # print "HEAP: " + str(h)
            ht = heapq.heappop(h)
            e = self.phrases[ht.scoreIndex]
            ret.append(e)
            # print "adding: " + str(e)

            if ht.begin < ht.scoreIndex:
                q = self.segTree.query(ht.begin, ht.scoreIndex - 1)
                item = Range(ht.begin, ht.scoreIndex, q[0], q[1])
                heapq.heappush(h, item)

            if ht.scoreIndex + 1 < ht.end:
                q = self.segTree.query(ht.scoreIndex + 1, ht.end - 1)
                item = Range(ht.scoreIndex + 1, ht.end, q[0], q[1])
                heapq.heappush(h, item)

        return ret


def test():
    """
    How to perform a category based auto-complete?

    Insert entries such as:
    sports$<PHRASE>, <SCORE>
    finance$<PHRASE>, <SCORE>
    politics$<PHRASE>, <SCORE>
    etc...
    where, (sports,finance,politics) are category names.

    While doing an auto-complete for phrase <PHRASE>, prepend the category
    name to it and separate the 2 with a $ (dollar) sign

    On my laptop (2.40GHz), without using psyco, this thing can do
    1000 lookups/sec (with k=10) with 3M candidates to choose
    from for that prefix!!

    However, RAM usage is a okay at 340MB. (got it down from about
    750MB by using __slots__ for the Entry class).
    """

    from datetime import datetime as dt

    ac = AutoComplete()
    ac.insert(Entry("how i met your mother", 10))
    ac.insert(Entry("how i got to know you", 5))
    ac.insert(Entry("how i found out about her", 25))
    ac.insert(Entry("where do you go?", 10))
    ac.insert(Entry("what have you been up to these days?", 80))
    ac.insert(Entry("how i made an aircraft", 11))
    ac.insert(Entry("another day another time", 6))
    ac.insert(Entry("gabriel weinberg", 6))

    # for i in xrange(0, 3000000):
    #     ac.insert(Entry("how i met your mother", i))

    # For testing
    AutoComplete.NAIVE_ALGO_THRESHOLD_MULTIPLIER = 0

    ac.commit()

    print "phrases:", ac.phrases
    print ac.query("d", 4)
    print ac.query("a", 4)
    print ac.query("g", 4)
    print ac.query("h", 4)

    # print dt.now()
    # for i in xrange(0, 1000):
    #     ac.query("how i", 10)
    # print dt.now()




if __name__ == "__main__":
    test()

