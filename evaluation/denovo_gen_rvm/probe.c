#include "common.h"
#include "kmer_hash.h"
#include "packingDNAseq.h"
#include "probe.h"

int probe(gen_state_t *state, FILE *out)
{
    rvm_txid_t txid;
    probe_state_t *pstate;

    assert(state->phase == PROBE);

    if(state->pstate == NULL) {
        TX_START(txid);
        state->pstate = rvm_alloc(cfg, sizeof(probe_state_t));
        CHECK_ERROR(state->pstate == NULL,
                ("Failed to allocate probe state\n"));
        pstate = (probe_state_t*)state->pstate;
        pstate->skmerx = 0;
        pstate->curStartNode = state->start_list;
        pstate->posInContig = 0;
        pstate->floc = 0;
        TX_COMMIT(txid);
    } else {
        /* Seek to the correct output location in file */
        fseek(out, pstate->floc, SEEK_SET);
    }

    char unpackedKmer[KMER_LENGTH+1];

    while (pstate->curStartNode != NULL ) {
        /* Convenience alias */
        kmer_t *cur_kmer_ptr = pstate->curStartNode->kmerPtr;

        if(pstate->posInContig == 0) {
            TX_START(txid);

            /* Need to unpack the seed first */
            unpackSequence((unsigned char*) cur_kmer_ptr->kmer,
                    (unsigned char*) unpackedKmer, KMER_LENGTH);

            /* Initialize current contig with the seed content */
            memcpy(pstate->cur_contig, unpackedKmer, KMER_LENGTH * sizeof(char));
            pstate->posInContig = KMER_LENGTH;

            TX_COMMIT(txid);
        }

       /* Keep adding bases while not finding a terminal node */
       char right_ext = cur_kmer_ptr->r_ext;
       while (right_ext != 'F') {
           TX_START(txid);

           pstate->cur_contig[pstate->posInContig] = right_ext;
           pstate->posInContig++;
           /* At position cur_contig[posInContig-KMER_LENGTH] starts the
            * last k-mer in the current contig */
           unsigned char *nextKmer = (unsigned char *)
                   &(pstate->cur_contig[pstate->posInContig - KMER_LENGTH]);
           cur_kmer_ptr = lookup_kmer(state->tbl, nextKmer);
           CHECK_ERROR(cur_kmer_ptr == NULL,
                   ("Couldn't find next kmer in hash table\n"));

           right_ext = cur_kmer_ptr->r_ext;

           TX_COMMIT(txid);
       }

       /* Move on to the next contig */
       TX_START(txid);
       /* Print the contig since we have found the corresponding terminal node */
       pstate->cur_contig[pstate->posInContig] = '\0';
       pstate->floc += fprintf(out,"%s\n", pstate->cur_contig);

       /* Move to the next start node in the list */
       pstate->curStartNode = pstate->curStartNode->next;
       pstate->posInContig = 0;
       TX_COMMIT(txid);
    }

    return 1;
}

void probe_state_free(void *pstate)
{
    rvm_free(cfg, pstate);
}
