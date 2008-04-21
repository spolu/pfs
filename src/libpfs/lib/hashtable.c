#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "hashtable.h"

static int is_prime(long number);
static long calc_ideal_num_buck (ht_t *ht);




ht_t *
ht_create (long num_buck) 
{
  ht_t *ht;
  int i;

  assert(num_buck > 0);

  ht = (ht_t *) malloc (sizeof (ht_t));
  if (ht == NULL)
    return NULL;

  ht->bucks = (ht_pair_t **)
    malloc (num_buck * sizeof (ht_pair_t *));

  if (ht->bucks == NULL) {
    free(ht);
    return NULL;
  }

  ht->num_buck = num_buck;
  ht->num_elem = 0;

  for (i = 0; i < num_buck; i++)
    ht->bucks[i] = NULL;

  ht->ideal_ratio = 3.0;
  ht->lower_threshold = 0.0;
  ht->upper_threshold = 15.0;

  ht->key_cmp = ht_ptr_cmp;
  ht->hash = ht_str_hash;
  ht->key_dealloc = NULL;
  ht->value_dealloc = NULL;

  return ht;
}

void ht_destroy (ht_t *ht) 
{
  int i;

  for (i = 0; i < ht->num_buck; i++) 
    {
      ht_pair_t *pair = ht->bucks[i];
      while (pair != NULL) 
        {
	  ht_pair_t *next_pair = pair->next;
	  if (ht->key_dealloc != NULL)
	    ht->key_dealloc ((void *) pair->key);
	  if (ht->value_dealloc != NULL)
	    ht->value_dealloc(pair->value);
	  free (pair);
	  pair = next_pair;
        }
    }

  free(ht->bucks);
  free(ht);
}

int ht_has_key (const ht_t *ht, const void *key) 
{
  return (ht_get (ht, key) != NULL);
}

int ht_put (ht_t *ht, const void *key, void *value) 
{
  long hash_val;
  ht_pair_t *pair;

  assert(key != NULL);
  assert(value != NULL);

  hash_val = ht->hash (key) % ht->num_buck;
  pair = ht->bucks[hash_val];

  while (pair != NULL && ht->key_cmp (key, pair->key) != 0)
    pair = pair->next;

  if (pair) 
    {
      if (pair->key != key) 
        {
	  if (ht->key_dealloc != NULL)
	    ht->key_dealloc ((void *) pair->key);
	  pair->key = key;
        }
      if (pair->value != value) 
        {
	  if (ht->value_dealloc != NULL)
	    ht->value_dealloc (pair->value);
	  pair->value = value;
        }
    }
  else 
    {
      ht_pair_t *new_pair = (ht_pair_t *) malloc (sizeof (ht_pair_t));
      new_pair->key = key;
      new_pair->value = value;
      new_pair->next = ht->bucks[hash_val];
      ht->bucks[hash_val] = new_pair;
      ht->num_elem ++;
      
      if (ht->upper_threshold > ht->ideal_ratio) 
	{
	  float elem_buck_ratio = (float) ht->num_elem /
	    (float) ht->num_buck;
	  if (elem_buck_ratio > ht->upper_threshold) {
	    ht_rehash (ht, 0);
	  }
	}
    }
  
  return 0;
}

void *ht_get (const ht_t *ht, const void *key) 
{
  long hash_val = ht->hash (key) % ht->num_buck;
  ht_pair_t *pair = ht->bucks[hash_val];

  while (pair != NULL && ht->key_cmp(key, pair->key) != 0)
    pair = pair->next;

  return (pair == NULL) ? NULL : pair->value;
}

void ht_remove (ht_t *ht, const void *key) 
{
  long hash_val = ht->hash (key) % ht->num_buck;
  ht_pair_t *pair = ht->bucks [hash_val];
  ht_pair_t *prev_pair = NULL;

  while (pair != NULL && ht->key_cmp(key, pair->key) != 0) 
    {
      prev_pair = pair;
      pair = pair->next;
    }

  if (pair != NULL) 
    {
      if (ht->key_dealloc != NULL)
	ht->key_dealloc ((void *) pair->key);
      if (ht->value_dealloc != NULL)
	ht->value_dealloc (pair->value);
      if (prev_pair != NULL)
	prev_pair->next = pair->next;
      else
	ht->bucks[hash_val] = pair->next;
      free (pair);
      ht->num_elem --;

      if (ht->lower_threshold > 0.0) 
        {
	  float elem_buck_ratio = (float) ht->num_elem /
	    (float) ht->num_buck;
	  if (elem_buck_ratio < ht->lower_threshold)
	    ht_rehash (ht, 0);
        }
    }
}

void ht_remove_all (ht_t *ht) 
{
  int i;

  for (i = 0; i < ht->num_buck; i++) 
    {
      ht_pair_t *pair = ht->bucks[i];
      while (pair != NULL) 
        {
	  ht_pair_t *next_pair = pair->next;
	  if (ht->key_dealloc != NULL)
	    ht->key_dealloc ((void *) pair->key);
	  if (ht->value_dealloc != NULL)
	    ht->value_dealloc (pair->value);
	  free(pair);
	  pair = next_pair;
        }
      ht->bucks[i] = NULL;
    }

  ht->num_elem = 0;
  ht_rehash (ht, 5);
}

int ht_empty (const ht_t *ht) 
{
  return (ht->num_elem == 0);
}

long ht_size (const ht_t *ht) 
{
  return ht->num_elem;
}

long ht_num_buck (const ht_t *ht) 
{
  return ht->num_buck;
}

void ht_set_key_cmp (ht_t *ht,
		     int (*key_cmp)(const void *key1, const void *key2)) 
{
  assert(key_cmp != NULL);
  ht->key_cmp = key_cmp;
}

void ht_set_hash (ht_t *ht,
		  unsigned long (*hash)(const void *key))
{
  assert(hash != NULL);
  ht->hash = hash;
}

void ht_rehash (ht_t *ht, long num_buck) 
{
  ht_pair_t **new_bucks;
  int i;

  assert(num_buck >= 0);
  if (num_buck == 0)
    num_buck = calc_ideal_num_buck (ht);

  if (num_buck == ht->num_buck)
    return; /* already the right size! */

  new_bucks = (ht_pair_t **)
    malloc (num_buck * sizeof (ht_pair_t *));
  if (new_bucks == NULL) 
    {
      /* Couldn't allocate memory for the new array.  This isn't a fatal
       *error; we just can't perform the rehash. */
      return;
    }

  for (i = 0; i < num_buck; i++)
    new_bucks[i] = NULL;

  for (i = 0; i < ht->num_buck; i++) 
    {
      ht_pair_t *pair = ht->bucks[i];
      while (pair != NULL) 
        {
	  ht_pair_t *next_pair = pair->next;
	  long hash_val = ht->hash (pair->key) % num_buck;
	  pair->next = new_bucks[hash_val];
	  new_bucks[hash_val] = pair;
	  pair = next_pair;
        }
    }

  free (ht->bucks);
  ht->bucks = new_bucks;
  ht->num_buck = num_buck;
}

void ht_set_ratio(ht_t *ht, float ideal_ratio,
		  float lower_threshold, float upper_threshold) 
{
  assert(ideal_ratio > 0.0);
  assert(lower_threshold < ideal_ratio);
  assert(upper_threshold == 0.0 || upper_threshold > ideal_ratio);

  ht->ideal_ratio = ideal_ratio;
  ht->lower_threshold = lower_threshold;
  ht->upper_threshold = upper_threshold;
}

void ht_set_dealloc (ht_t *ht,
		     void (*key_dealloc)(void *key),
		     void (*value_dealloc)(void *value)) 
{
  ht->key_dealloc = key_dealloc;
  ht->value_dealloc = value_dealloc;
}

unsigned long ht_str_hash (const void *key) 
{
  const unsigned char *str = (const unsigned char *) key;
  unsigned long hash_val = 0;
  int i;

  for (i = 0; str[i] != '\0'; i ++)
    hash_val = hash_val * 37 + str[i];

  return hash_val;
}

int ht_int_cmp (const void *v1, const void *v2)
{
  return (*((int *)v1) != *((int *)v2));
}

int ht_ptr_cmp(const void *ptr1, const void *ptr2) 
{
  return (ptr1 != ptr2);
}

unsigned long ht_ptr_hash(const void *ptr) 
{
  return ((unsigned long) ptr) >> 4;
}

static int is_prime(long number)
{
  long i;

  for (i = 3; i < 51; i += 2)
    if (number == i)
      return 1;
    else if (number % i == 0)
      return 0;

  return 1; /* maybe */
}

static long calc_ideal_num_buck (ht_t *ht)
{
  long ideal_num_buck = ht->num_elem / ht->ideal_ratio;
  if (ideal_num_buck < 5)
    ideal_num_buck = 5;
  else
    ideal_num_buck |= 0x01; /* make it an odd number */
  while (!is_prime(ideal_num_buck))
    ideal_num_buck += 2;

  return ideal_num_buck;
}
