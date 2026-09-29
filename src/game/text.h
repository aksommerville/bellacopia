/* text.h
 * Odds and ends for text processing.
 */
 
#ifndef BELLACOPIA_TEXT_H
#define BELLACOPIA_TEXT_H

/* Produce a string like "negative one hundred forty-three".
 * If we ever support other languages, it's my problem.
 */
int int_as_words(char *dst,int dsta,int v);

#endif
