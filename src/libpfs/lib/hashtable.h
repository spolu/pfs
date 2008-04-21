
#ifndef _HASHTABLE_H
#define _HASHTABLE_H


typedef struct ht_pair
{
  const void *key;
  void *value;
  struct ht_pair *next;
} ht_pair_t;

typedef struct 
{
  long num_buck;
  long num_elem;
  ht_pair_t **bucks;
  float ideal_ratio, lower_threshold, upper_threshold;
  int (*key_cmp) (const void *key1, const void *key2);
  unsigned long (*hash) (const void *key);
  void (*key_dealloc) (void *key);
  void (*value_dealloc) (void *value);
} ht_t;

ht_t *ht_create (long num_buck);
void ht_destroy (ht_t *ht);

int ht_has_key (const ht_t *ht, const void *key);
int ht_put (ht_t *ht, const void *key, void *value);
void *ht_get (const ht_t *ht, const void *key);
void ht_remove (ht_t *ht, const void *key);
void ht_remove_all (ht_t *ht);

int ht_empty (const ht_t *ht);
long ht_size (const ht_t *ht);
long ht_num_buck (const ht_t *ht);

void ht_set_key_cmp (ht_t *ht, int (*key_cmp)(const void *key1, const void *key2));
void ht_set_hash (ht_t *ht, unsigned long (*hash)(const void *key));

void ht_rehash (ht_t *ht, long num_buck);
void ht_set_ratio(ht_t * ht, float ideal_ratio,
		  float lower_threshold,
		  float upper_threshold);

void ht_set_dealloc(ht_t *ht,
		    void (*key_dealloc)(void *key),
		    void (*value_dealloc)(void *value));

unsigned long ht_str_hash (const void *key);
unsigned long ht_ptr_hash (const void *ptr);
int ht_ptr_cmp(const void *ptr1, const void *ptr2);
int ht_int_cmp (const void *v1, const void *v2);

#endif /* _HASHTABLE_H */


