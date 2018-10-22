/*
 * Overview of cache strategy, cache 3 most popular objects and other recently-used
*/
#include "csapp.h"
#include "cache.h"


/*
 * Declaration of the methods and variables that only used
 * in cache.c
 */
static cache_block *new_cache(char *id, char *content, 
				unsigned int block_size, unsigned int freq);
static cache_block *find_cache(cache_list *cl, char *id);
static void move_to_head(cache_list *cl, cache_block *cb);
static void insert_cache(cache_list *cl, cache_block *cb);
static void delete_cache(cache_list *cl, cache_block *new_cb);
static cache_block *delete_cache_block(cache_list *cl, cache_block *cb);


/*
 * Initialize caceh list
 */
void init_cache_list(cache_list *cl)
{
	printf("----FUNCTION: init_cache_list----\n");
	cl->total_size = 0;

	/* initialize the two cache block as head and tail */
	cl->head = new_cache(NULL, NULL, 0, 0);
	cl->tail = new_cache(NULL, NULL, 0, 0);

	cl->head->next = cl->tail;
	cl->tail->prev = cl->head;

	return;
}

/*
 * Free cache list
 */
void free_cache_list(cache_list *cl)
{
	printf("----FUNCTION: free_cache_list----\n");
	/* delete all cache block */
	cache_block *cb;
	for(cb = cl->tail->prev; cb != cl->head;)
	{
		cb = delete_cache_block(cl, cb);
	}

	/* free heap */
	free(cl->head);
	free(cl->tail);
	free(cl);
	return;
}

/*
 * Create a new cache block
 */
static cache_block *new_cache(char *id, char *content, 
				unsigned int block_size, unsigned int freq)
{
	printf("----FUNCTION: new_cache----\n");
	cache_block *cb;
	cb = (cache_block *)malloc(sizeof(cache_block));

	/* 
	 * copy cache id, if id == NULL, 
	 * it is header and tail
	 */
	if (id != NULL)
	{
		cb->id = (char *) malloc(sizeof(char) * (strlen(id) + 1));
		strcpy(cb->id, id);
	}

	cb->block_size = block_size;
	cb->freq = freq;

	/* 
	 * copy cache content, if content == NULL, 
	 * it is header and tail
	 */
	if (content != NULL)
	{
		cb->content = (char *) malloc(sizeof(char) * block_size);
		memcpy(cb->content, content, sizeof(char) * block_size);
	}

	cb->prev = NULL;
	cb->next = NULL;

	return cb;
}

/*
 * Check cache list, if there exist the request content,
 * read from it.
 */
char* read_cache(cache_list *cl, char *id, int* size)
{
	printf("----FUNCTION: read_cache----\n");
	printf("The cache id is %s\n", id);
	cache_block *cache = NULL;

	pthread_mutex_lock(&mutex_lock);
	char* content_copy;

	cache = find_cache(cl, id);

	/* if cache hit, copy the content */
	if (cache != NULL)
	{
		*size = cache->block_size;
		content_copy = (char*) malloc(sizeof(char)*cache->block_size);

		memcpy(content_copy, cache->content,
			   sizeof(char) * cache->block_size);
		pthread_mutex_unlock(&mutex_lock);
        return content_copy;

	}
	
	pthread_mutex_unlock(&mutex_lock);
	return NULL;
}

/*
 * Find a cache block in cache list by id
 */
cache_block *find_cache(cache_list *cl, char *id)
{
	printf("----FUNCTION: find_cache----\n");
	cache_block *cb;

	/*
	 * serach the cache list, if there is a hit, move
	 * the cache block to the head of cache list
	 */
	for(cb = cl->head->next; cb != cl->tail; cb = cb->next)
	{
    	if(!strcmp(cb->id, id))
    	{
    		move_to_head(cl, cb);
    		return cb; 
    	} 			
    }
    return NULL;
}

/*
 * put the new cache block to the head of the cache list.
 */
static void move_to_head(cache_list *cl, cache_block *cb)
{
	printf("----FUNCTION: move_to_head----\n");
	/* put cache block to the head */
	cb->next->prev = cb->prev;
	cb->prev->next = cb->next;
	cb->prev = cl->head;
	cb->next = cl->head->next;
	cl->head->next = cb;
	cb->next->prev = cb;
	cb->freq++;
	return;
}

/*
 * Insert a cache block into cache list
 */
static void insert_cache(cache_list *cl, cache_block *cb)
{
	printf("----FUNCTION: insert_cache----\n");
	/* manipulate pointer */
	cb->prev = cl->head;
	cb->next = cl->head->next;
	cl->head->next->prev = cb;
	cl->head->next = cb;

    /* change total size */
	cl->total_size += cb->block_size;
	return;
}

/*
 * Write a new cache block to cache list
 */
void update_cache(cache_list *cl, char *id, char *content,  
				  unsigned int block_size){
	printf("----FUNCTION: update_cache----\n");
	cache_block *new_cb = NULL;

	pthread_mutex_lock(&mutex_lock);
	new_cb = new_cache(id, content, block_size, 1);

    /* 
     * When there is enough room, insert the cache,
	 * else delete old cache block.
     */
    if(cl->total_size + block_size <= MAX_CACHE_SIZE)
    {
    	insert_cache(cl, new_cb);
    }
    else
    {
    	delete_cache(cl, new_cb);
    	insert_cache(cl, new_cb);
    }

    pthread_mutex_unlock(&mutex_lock);
    return;
}

/*
 * Write a new cache block to cache list
 */
static void delete_cache(cache_list *cl, cache_block *new_cb){
	printf("----FUNCTION: delete_cache----\n");

	cache_block *cb = cl->tail->prev;
	int freq = 1;

	/* Delete from tail from frequency 1, make room for new cache block */
	while(cl->total_size + new_cb->block_size <= MAX_CACHE_SIZE)
	{
		if(cb->freq==freq){
			cb = delete_cache_block(cl,cb);
		}else{
			cb = cb->prev;
		}
		if(cb==cl->head)
		{
			freq++;
			cb = cl->tail->prev;
		}
	}
}

static cache_block *delete_cache_block(cache_list *cl, cache_block *cb){
	printf("----FUNCTION: delete_cache_block----\n");
	cache_block *prev_cb;
	cb->next->prev = cb->prev;
	cb->prev->next = cb->next;
	cl->total_size -= cb->block_size;
	prev_cb = cb->prev;

	cb->prev = NULL;
	cb->next = NULL;

	/* Free heap */
	free(cb->id);
	free(cb->content);
	free(cb);

	return prev_cb;
}