#ifndef _REC_H_
#define _REC_H_

#define REC_FNAME "gen_assemble.rec"
#define REC_TMP_FNAME ".gen_assemble.rec"

/* File descriptor used for the recoverable region */
extern int rec_fd;

/* Pointer to the beginning of the recoverable region */
extern void *rec_reg;

#endif //_REC_H_
