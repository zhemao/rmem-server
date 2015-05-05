#ifndef PACKING_DNA_SEQ_H
#define PACKING_DNA_SEQ_H

void init_LookupTable();

unsigned char convertFourMerToPackedCode(unsigned char *fourMer);

void packSequence(const unsigned char *seq_to_pack, unsigned char *m_data, int m_len);

void unpackSequence(const unsigned char *seq_to_unpack, unsigned char *unpacked_seq, int kmer_len);

int comparePackedSeq(const unsigned char *seq1, const unsigned char *seq2, int seq_len);

#endif // PACKING_DNA_SEQ_H

