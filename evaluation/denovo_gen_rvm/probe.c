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
        pstate->curStartNode = state->start_list;
        pstate->cur_kmr = state->start_list->kmerPtr;
        pstate->posInContig = 0;
        pstate->floc = 0;
        pstate->nkmer_proc = 0;
        TX_COMMIT(txid);

        printf("Allocated probe state\n");

    } else {
        pstate = (probe_state_t*)state->pstate;
        printf("Probe restarting from kmer %ld\n", pstate->nkmer_proc);

        /* Seek to the correct output location in file */
        fseek(out, pstate->floc, SEEK_SET);
    }

    char unpackedKmer[KMER_LENGTH+1];

    while (pstate->curStartNode != NULL ) {
        
        /* Starting a new contig. This may not be true if we are recovering
           from a failure. */
        if(pstate->posInContig == 0) {
            TX_START(txid);

            pstate->cur_kmr = pstate->curStartNode->kmerPtr;

            /* Need to unpack the seed first */
            unpackSequence((unsigned char*) pstate->cur_kmr->kmer,
                    (unsigned char*) unpackedKmer, KMER_LENGTH);

            /* Initialize current contig with the seed content */
            memcpy(pstate->cur_contig, unpackedKmer, KMER_LENGTH * sizeof(char));
            pstate->posInContig = KMER_LENGTH;

            pstate->nkmer_proc++;

            if(pstate->nkmer_proc % PRINT_FREQ == 0) 
               printf("Probe kmer %ld\n", pstate->nkmer_proc);

            if(pstate->nkmer_proc % cp_freq == 0) {
                TX_COMMIT(txid);
            }

            if(pstate->nkmer_proc % fa_freq == 0) {
                printf("Simulating probe failure after %ldth kmer\n", pstate->nkmer_proc);
                exit(EXIT_FAILURE);
            }
        }

       /* Keep adding bases until we find a terminal node */
       char right_ext = pstate->cur_kmr->r_ext;
       while (right_ext != 'F') {
           TX_START(txid);

           pstate->cur_contig[pstate->posInContig] = right_ext;
           pstate->posInContig++;
           /* At position cur_contig[posInContig-KMER_LENGTH] starts the
            * last k-mer in the current contig */
           unsigned char *nextKmer = (unsigned char *)
                   &(pstate->cur_contig[pstate->posInContig - KMER_LENGTH]);
           pstate->cur_kmr = lookup_kmer(state->tbl, nextKmer);
           CHECK_ERROR(pstate->cur_kmr == NULL,
                   ("Couldn't find kmer %ld in hash table\n", pstate->nkmer_proc));

           right_ext = pstate->cur_kmr->r_ext;

           pstate->nkmer_proc++;

           if(pstate->nkmer_proc % PRINT_FREQ == 0) 
               printf("Probe kmer %ld\n", pstate->nkmer_proc);

           if(pstate->nkmer_proc % cp_freq == 0) {
               TX_COMMIT(txid);
           }

           if(pstate->nkmer_proc % fa_freq == 0) {
               printf("Simulating probe failure after %ldth kmer\n", pstate->nkmer_proc);
               exit(EXIT_FAILURE);
           }
       }

       /* Move on to the next contig */
       /* Print the contig since we have found the corresponding terminal node */
       pstate->cur_contig[pstate->posInContig] = '\0';
       pstate->floc += fprintf(out,"%s\n", pstate->cur_contig);

       /* Move to the next start node in the list */
       pstate->curStartNode = pstate->curStartNode->next;
       pstate->posInContig = 0;
   }

    return 1;
}

void probe_state_free(void *pstate)
{
    rvm_free(cfg, pstate);
}
