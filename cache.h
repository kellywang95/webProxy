/*
 * cache.h
 * To combine LFU and LRU, when we need to make room for new block,
 * we delete cache block from tail from frequency 1. If it goes to 
 * the head, increment the frequency and loop from the tail again.
*/

#ifndef CACHE_H
#define CACHE_H

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#include "csapp.h"

/* Definition of cache block */
typedef struct cacheblock{
	char *id;
	unsigned int block_size;
	unsigned int freq;
	char *content;
	struct cacheblock *prev;
	struct cacheblock *next;
}cache_block;

/* Definition of cache list */
typedef struct
{
	unsigned int total_size;
	cache_block *head;
	cache_block *tail;
}cache_list;

pthread_mutex_t mutex_lock;

void init_cache_list(cache_list *cl);
char* read_cache(cache_list *cl, char *id, int* size);
void update_cache(cache_list *cl, char *id, char *content,  
				  unsigned int block_size);
void free_cache_list(cache_list *cl);


#endif