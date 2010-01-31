/*
 * Copyright (c) 2009 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <string.h>

unsigned int hash_string(const void *key)
{
    const char *str = (const char *)key;
    unsigned int hash = 5381;
    unsigned int c;

    while((c = (unsigned int)*str))
    {
        hash = ((hash << 5) + hash) + c;
        str++;
    }

    return hash;
}

int string_equal(const void *key1, const void *key2)
{
    return strcmp((const char *)key1, (const char *)key2) == 0;
}
