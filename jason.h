//
//  jason.h
//
//  Created by Timothy Smale on 10/10/2015.
//  Copyright (c) 2015 Timothy Smale. All rights reserved.
//

#ifndef JASON_H
#define JASON_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
    
    typedef void*(*jasonMallocCb_t)(size_t*);
    typedef void(*jasonFreeCb_t)(void*);
    typedef void*(*jasonReallocCb_t)(void*, size_t*);
    typedef uint32_t(*jasonHashCb_t)(char*, size_t);
    
    typedef enum
    {
        jasonStatus_Finished = 1,
        jasonStatus_Continue = 0,
        jasonStatus_UnexpectedCharacter = -1,
        jasonStatus_ExpectedObjectKey = -2,
        jasonStatus_UnexpectedEndOfString = -3,
        jasonStatus_OutOfMemory = -4,
        jasonStatus_IntegerOverflow = -5,
    }
    jasonStatus;
    
    typedef enum
    {
        jasonValueType_Number,
        jasonValueType_Object,
        jasonValueType_Array,
        jasonValueType_String,
        jasonValueType_True,
        jasonValueType_False,
        jasonValueType_Null
    }
    jasonValueType;
    
    typedef struct
    {
        const char *Value;
        union
        {
            int32_t ValueLen;	// only used by Values
            int32_t Parent; // used by Keys only
        };
        
        int32_t Next;
    }
    jasonValue;
    
    typedef struct
    {
        int32_t *Buckets;
        int32_t NumBuckets;
        int32_t NumKeys;
    }
    jasonHashTable;
    
    typedef struct
    {
        jasonHashTable KeyLookupTable;
        jasonMallocCb_t Malloc;
        jasonFreeCb_t Free;
        jasonHashCb_t Hash;
        jasonValue *RootValue;
        const char *ParsePosition;
        int32_t NumValues;
        int32_t MaxValues;
    }
    jason;
    
#define JASON_INCSTR(pointer, end) pointer++; if(pointer >= end) { return jasonStatus_UnexpectedEndOfString; }
#define JASON_INCVAL(pointer, end) { pointer++; if(pointer >= end) { return jasonStatus_OutOfMemory; } }
#define JASON_SKIPWHITESPACE(pointer, end) \
while(pointer < end && (*pointer == ' ' || *pointer == '\n' || *pointer == '\t' || *pointer == '\r')) \
    { \
        pointer++; \
    }
    
#define JASON_SETOFFSET(dest, len) \
    { \
        if(len >= INT_MAX || len <= INT_MIN) \
        { \
            return jasonStatus_Break(jasonStatus_IntegerOverflow); \
        } \
        else \
        { \
            dest = (int32_t)(len); \
        } \
    }
    
#define JASON_STRINGIFY_CASE(str) case str: return #str
    
    const char *jasonStatus_Describe(jasonStatus status)
    {
        switch(status)
        {
                JASON_STRINGIFY_CASE(jasonStatus_IntegerOverflow);
                JASON_STRINGIFY_CASE(jasonStatus_Continue);
                JASON_STRINGIFY_CASE(jasonStatus_OutOfMemory);
                JASON_STRINGIFY_CASE(jasonStatus_Finished);
                JASON_STRINGIFY_CASE(jasonStatus_ExpectedObjectKey);
                JASON_STRINGIFY_CASE(jasonStatus_UnexpectedCharacter);
                JASON_STRINGIFY_CASE(jasonStatus_UnexpectedEndOfString);
        }
        
        return "";
    }
    
    const char *jasonValueType_Describe(jasonValueType type)
    {
        switch(type)
        {
                JASON_STRINGIFY_CASE(jasonValueType_Null);
                JASON_STRINGIFY_CASE(jasonValueType_False);
                JASON_STRINGIFY_CASE(jasonValueType_Array);
                JASON_STRINGIFY_CASE(jasonValueType_Number);
                JASON_STRINGIFY_CASE(jasonValueType_Object);
                JASON_STRINGIFY_CASE(jasonValueType_String);
                JASON_STRINGIFY_CASE(jasonValueType_True);
        }
        
        return "";
    }
    
    jasonStatus jasonStatus_Break(jasonStatus status)
    {
        return status;
    }
    
    jasonValueType jasonValue_GetType(jasonValue *value)
    {
        switch(value->Value[0])
        {
            case '{':
                return jasonValueType_Object;
                
            case '[':
                return jasonValueType_Array;
                
            case '"':
                return jasonValueType_String;
                
            case 'f':
                return jasonValueType_False;
                
            case 't':
                return jasonValueType_True;
                
            case 'n':
                return jasonValueType_Null;
                
            default:
                return jasonValueType_Number;
        }
    }
    
    const char *jasonValue_GetValue(jasonValue *value)
    {
        jasonValueType type = jasonValue_GetType(value);
        if(type == jasonValueType_String)
        {
            return value->Value + 1;
        }
        else
        {
            return value->Value;
        }
    }
    
    int32_t jasonValue_GetValueLen(jasonValue *value)
    {
        jasonValueType type = jasonValue_GetType(value);
        
        switch(type)
        {
            case jasonValueType_String:
                return value->ValueLen - 2;
                
            case jasonValueType_Object:
            case jasonValueType_Array:
                return 1;
                
            default:
                return value->ValueLen;
        }
    }
    
    jasonValue *jasonValue_GetFirstChild(jasonValue *parent)
    {
        jasonValueType type = jasonValue_GetType(parent);
        if((type == jasonValueType_Object || type == jasonValueType_Array) && parent->ValueLen > 1)
        {
            return parent + 1;
        }
        
        return NULL;
    }
    
    jasonValue *jasonValue_GetNextSibling(jasonValue *value)
    {
        if(value->Next != 0)
        {
            return value + value->Next;
        }
        
        return NULL;
    }
    
    uint32_t jason_HashKey(jason *jason, jasonValue *parent, const char *key, int32_t keyLen)
    {
        uint32_t parentHash = jason->Hash((char*)parent->Value, sizeof(intptr_t));
        uint32_t keyHash = jason->Hash((char*)key, keyLen);
        parentHash ^= keyHash + 0x9e3779b9 + (parentHash << 6) + (parentHash >> 2);
        
        return parentHash;
    }
    
    jasonStatus jason_HashInsertDirect(jason *jason, jasonHashTable *table, jasonValue *val, uint32_t hash)
    {
        table->NumKeys++;
        uint32_t bucketIndex = hash % table->NumBuckets;
        int32_t keyIndex = table->Buckets[bucketIndex];
        if(keyIndex != 0)
        {
            jasonValue *key = jason->RootValue + keyIndex;
            JASON_SETOFFSET(val->Next, (key - val));
        }
        
        JASON_SETOFFSET(table->Buckets[bucketIndex], (val - jason->RootValue));
        
        return jasonStatus_Continue;
    }
    
    jasonStatus jason_HashInsert(jason *jason, jasonValue *val, jasonValue *parent, const char *keyStr, int32_t keyLen)
    {
        val->Next = 0;
        JASON_SETOFFSET(val->Parent, (parent - val));
        
        if(jason->KeyLookupTable.NumBuckets <= jason->KeyLookupTable.NumKeys)
        {
            jasonHashTable newTable;
            memset(&newTable, 0, sizeof(jasonHashTable));

            size_t memLength = (jason->KeyLookupTable.NumKeys + 32) * sizeof(int32_t);
            newTable.Buckets = jason->Malloc(&memLength);
            newTable.NumBuckets = (int32_t)(memLength / (sizeof(int32_t)));
            
            if(newTable.NumBuckets == 0)
            {
                return jasonStatus_Break(jasonStatus_OutOfMemory);
            }
            
            memset(newTable.Buckets, 0, memLength);
            
            if(jason->KeyLookupTable.Buckets)
            {
                for(size_t i = 0; i < jason->KeyLookupTable.NumBuckets; i++)
                {
                    while(jason->KeyLookupTable.Buckets[i] != 0)
                    {
                        jasonValue *key = jason->RootValue + jason->KeyLookupTable.Buckets[i];
                        jason->KeyLookupTable.Buckets[i] = key->Next;
                        // re-insert
                        
                        uint32_t hash = jason_HashKey(jason, key + key->Parent, jasonValue_GetValue(key), jasonValue_GetValueLen(key));
                        jasonStatus status = jason_HashInsertDirect(jason, &newTable, key, hash);
                        if(status != jasonStatus_Continue)
                        {
                            return status;
                        }
                    }
                }
            }
            
            jason->Free(jason->KeyLookupTable.Buckets);
            memcpy(&jason->KeyLookupTable, &newTable, sizeof(jasonHashTable));
        }
        
        uint32_t hash = jason_HashKey(jason, parent, keyStr, keyLen);
        return jason_HashInsertDirect(jason, &jason->KeyLookupTable, val, hash);
    }
    
    jasonValue *jason_HashLookup(jason *jason, jasonValue *parent, const char *keyStr, int32_t keyLen)
    {
        if(jason->KeyLookupTable.NumBuckets == 0)
        {
            return NULL;
        }
        
        uint32_t hash = jason_HashKey(jason, parent, keyStr, keyLen);
        uint32_t bucketIndex = hash % jason->KeyLookupTable.NumBuckets;
        int32_t first = jason->KeyLookupTable.Buckets[bucketIndex];
        if(first != 0)
        {
            for(jasonValue *key = jason->RootValue + first; key != NULL; key = jasonValue_GetNextSibling(key))
            {
                jasonValue *keyParent = key + key->Parent;
                if(keyParent->Value == parent->Value) // same parent
                {
                    if(strncmp(jasonValue_GetValue(key), keyStr, keyLen) == 0) // same key string
                    {
                        return key + 1;
                    }
                }
            }
        }
        
        return NULL;
    }
    
    jasonStatus jason_DeserializeStep(jason *jason, const char *strEnd, jasonValue **value, jasonValue *valueEnd)
    {
        const char **str = &jason->ParsePosition;
        jasonValue *val = (*value);
        val->Value = *str;
        val->ValueLen = 0;
        val->Next = 0;
        jasonValueType type = jasonValue_GetType(val);
        
        switch (type)
        {
            case jasonValueType_Object:
            {
                int32_t numChildren = 0;
                
                JASON_INCSTR((*str), strEnd);
                JASON_SKIPWHITESPACE((*str), strEnd);
                
                jasonValue *last = NULL;
                while(**str != '}')
                {
                    JASON_INCVAL((*value), valueEnd);
                    jasonValue *child = *value;
                    jasonStatus status = jason_DeserializeStep(jason, strEnd, value, valueEnd);
                    if(status != jasonStatus_Continue)
                    {
                        *value = val;
                        return status;
                    }
                    
                    JASON_SETOFFSET(val->ValueLen, val->ValueLen + child->ValueLen);
                    numChildren++;
                    if(numChildren % 2 == 1)
                    {
                        if(jasonValue_GetType(child) != jasonValueType_String)
                        {
                            *str = child->Value;
                            return jasonStatus_Break(jasonStatus_ExpectedObjectKey);
                        }
                        
                        if(**str != ':')
                        {
                            return jasonStatus_Break(jasonStatus_UnexpectedCharacter);
                        }
                        
                        JASON_INCSTR((*str), strEnd);
                        JASON_SKIPWHITESPACE((*str), strEnd);
                        
                        if(**str == '}')
                        {
                            return jasonStatus_Break(jasonStatus_UnexpectedCharacter);
                        }
                        
                        status = jason_HashInsert(jason, child, val, jasonValue_GetValue(child), jasonValue_GetValueLen(child));
                        if(status != jasonStatus_Continue)
                        {
                            return status;
                        }
                    }
                    else
                    {
                        if(last != NULL)
                        {
                            JASON_SETOFFSET(last->Next, (child - last));
                        }
                        
                        last = child;
                        
                        if(**str == '}')
                        {
                            break;
                        }
                        else
                        {
                            if(**str != ',')
                            {
                                return jasonStatus_Break(jasonStatus_UnexpectedCharacter);
                            }
                            
                            ++(*str);
                            JASON_SKIPWHITESPACE((*str), strEnd);
                        }
                    }
                }
                
                (*str)++;
                
                break;
            }
                
            case jasonValueType_Array:
            {
                JASON_INCSTR((*str), strEnd);
                JASON_SKIPWHITESPACE((*str), strEnd);
                
                jasonValue *last = NULL;
                while(**str != ']')
                {
                    JASON_INCVAL((*value), valueEnd);
                    jasonValue *child = *value;
                    jasonStatus status = jason_DeserializeStep(jason, strEnd, value, valueEnd);
                    if(status != jasonStatus_Continue)
                    {
                        *value = val;
                        return status;
                    }
                    
                    if(last != NULL)
                    {
                        JASON_SETOFFSET(last->Next, (child - last));
                    }
                    
                    last = child;
                    
                    JASON_SETOFFSET(val->ValueLen, val->ValueLen + child->ValueLen);
                    if(**str == ']')
                    {
                        break;
                    }
                    else
                    {
                        if(**str != ',')
                        {
                            return jasonStatus_Break(jasonStatus_UnexpectedCharacter);
                        }
                        
                        ++(*str);
                        JASON_SKIPWHITESPACE((*str), strEnd);
                    }
                }
                
                (*str)++;
                
                break;
            }
                
            case jasonValueType_String:
            {
                JASON_INCSTR((*str), strEnd);
                
                while(**str != '"')
                {
                    if(**str == '\\' && *(*str + 1) == '"')
                    {
                        JASON_INCSTR((*str), strEnd);
                    }
                    
                    JASON_INCSTR((*str), strEnd);
                };
                
                (*str)++;
                JASON_SETOFFSET(val->ValueLen, *str - val->Value);
                
                break;
            }
                
            case jasonValueType_False:
            {
                if(strncmp(val->Value, "false", 5) != 0)
                {
                    return jasonStatus_Break(jasonStatus_UnexpectedCharacter);
                }
                
                val->ValueLen = 5;
                (*str) += 5;
                break;
            }
                
            case jasonValueType_True:
            {
                if(strncmp(val->Value, "true", 4) != 0)
                {
                    return jasonStatus_Break(jasonStatus_UnexpectedCharacter);
                }
                
                val->ValueLen = 4;
                (*str) += 4;
                break;
            }
                
            case jasonValueType_Null:
            {
                if(strncmp(val->Value, "null", 4) != 0)
                {
                    return jasonStatus_Break(jasonStatus_UnexpectedCharacter);
                }
                
                val->ValueLen = 4;
                (*str) += 4;
                break;
            }
                
            case jasonValueType_Number:
            {
                if(!isdigit(**str) && **str != '-')
                {
                    return jasonStatus_Break(jasonStatus_UnexpectedCharacter);
                }
                
                while(isdigit(**str) || **str == '.' || **str == 'E' || **str == 'e' || **str == '-')
                {
                    ++*str;
                }
                
                if(!isdigit((*str)[-1]))
                {
                    return jasonStatus_Break(jasonStatus_UnexpectedCharacter);
                }
                
                JASON_SETOFFSET(val->ValueLen, *str - val->Value);
                break;
            }
        }
        
        JASON_SKIPWHITESPACE((*str), strEnd);
        
        if(*str < strEnd)
        {
            if(*value >= valueEnd)
            {
                return jasonStatus_Break(jasonStatus_OutOfMemory);
            }
            
            return jasonStatus_Continue;
        }
        else
        {
            return jasonStatus_Finished;
        }
    }
    
    
    void jason_Cleanup(jason *jason)
    {
        jason->Free(jason->RootValue);
        jason->Free(jason->KeyLookupTable.Buckets);
        jason->MaxValues = 0;
        jason->NumValues = 0;
        jason->KeyLookupTable.NumBuckets = 0;
        jason->KeyLookupTable.NumKeys = 0;
    }
    
    void *jason_Malloc(size_t *size)
    {
        void *ret = malloc(*size);
        if(ret == NULL)
        {
            *size = 0;
        }
        
        return ret;
    }
    
    void jason_Free(void * ptr)
    {
        free(ptr);
    }
    
    void *jason_Realloc(void *ptr, size_t *size)
    {
        void *newPtr = realloc(ptr, *size);
        if(newPtr == NULL)
        {
            *size = 0;
        }
        
        return newPtr;
    }
    
    uint32_t jason_Hash(char *key, size_t bytes)
    {
        uint32_t hash = 5831;
        for(uint32_t i = 0; i < bytes; ++i)
        {
            hash = 33 * hash + key[i];
        }
        
        return hash;
    }
    
    jasonStatus jason_Deserialize(jason *jason, const char *json, int32_t jsonLen)
    {
        if(json == NULL || jsonLen <= 0)
        {
            return jasonStatus_Break(jasonStatus_UnexpectedEndOfString);
        }
        
        if(jason->Malloc == NULL || jason->Free == NULL)
        {
            jason->Malloc = jason_Malloc;
            jason->Free = jason_Free;
        }
        
        if(jason->Hash == NULL)
        {
            jason->Hash = jason_Hash;
        }
        
        // alloc initial memory
        jason->MaxValues = 32;
        size_t memLength = jason->MaxValues * sizeof(jasonValue);
        jason->RootValue = jason->Malloc(&memLength);
        
        if(jason->RootValue == NULL)
        {
            return jasonStatus_Break(jasonStatus_OutOfMemory);
        }
        
        jason->MaxValues = (int32_t)(memLength / sizeof(jasonValue));
        memset(jason->RootValue, 0, jason->MaxValues * sizeof(jasonValue));
        
        // begin
        jasonValue *valuesEnd = jason->RootValue + jason->MaxValues;
        jasonValue *valuesIt = jason->RootValue;
        const char *jsonBegin = json;
        const char *end = jsonBegin + jsonLen;
        jason->NumValues = 0;
        jason->ParsePosition = json;
        jasonStatus status = jasonStatus_Continue;
        
        while(status == jasonStatus_Continue)
        {
            jasonValue *lastValue = valuesIt;
            status = jason_DeserializeStep(jason, end, &valuesIt, valuesEnd);
            jason->NumValues += (int32_t)(valuesIt - lastValue);
            
            // more memory?
            if(status == jasonStatus_OutOfMemory)
            {
                int32_t valuesStep = (jason->MaxValues / 2);
                
                if(jason->MaxValues > INT_MAX - valuesStep)
                {
                    valuesStep = (INT_MAX - jason->MaxValues);
                    
                    if(valuesStep <= 0)
                    {
                        break;
                    }
                }
                
                int32_t newMaxValues = jason->MaxValues + valuesStep;
                size_t newMemLength = newMaxValues * sizeof(jasonValue);
                jasonValue *newRoot = jason->Malloc(&newMemLength);
                
                if(newMemLength <= memLength || abs((int)(memLength - newMemLength)) < sizeof(jasonValue))
                {
                    jason->Free(newRoot);
                    newRoot = NULL;
                }
                
                if(newRoot == NULL)
                {
                    return jasonStatus_Break(jasonStatus_OutOfMemory);
                }
                
                newMaxValues = (int32_t)(memLength / sizeof(jasonValue));
                
                memcpy(newRoot, jason->RootValue, jason->MaxValues * sizeof(jasonValue));
                memset(newRoot + (jason->MaxValues * sizeof(jasonValue)), 0, (newMaxValues - jason->NumValues) * sizeof(jasonValue));
                
                jason->Free(jason->RootValue);
                jason->RootValue = newRoot;
                jason->MaxValues = newMaxValues;
                valuesIt = jason->RootValue + jason->NumValues;
                valuesEnd = jason->RootValue + jason->MaxValues;
                memLength = newMemLength;
                
                // reset last value, in case we finished half-way through it
                jason->ParsePosition = valuesIt->Value;
                valuesIt--;
                status = jasonStatus_Continue;
            }
        }
        
        if(status == jasonStatus_Finished)
        {
            jason->ParsePosition = jsonBegin;
        }
        else
        {
            if(jason->RootValue != NULL)
            {
                jason->Free(jason->RootValue);
                jason->RootValue = NULL;
            }
        }
        
        return status;
    }
    
#ifdef __cplusplus
}
#endif

#endif
