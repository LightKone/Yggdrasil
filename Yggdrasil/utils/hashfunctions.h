/*******************************************************************************
*
* Taken from:
*
* bloom_filter.c -- A simple bloom filter implementation
* by timdoug -- timdoug@gmail.com -- http://www.timdoug.com -- 2008-07-05
* see http://en.wikipedia.org/wiki/Bloom_filter
*
********************************************************************************
*
* Copyright (c) 2008, timdoug(@gmail.com) -- except for the hash functions
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * The name of the author may not be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY timdoug ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL timdoug BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
********************************************************************************/

#ifndef CORE_HASHFUNCTIONS_H_
#define CORE_HASHFUNCTIONS_H_

#include <stdio.h>

unsigned int djb2(const void *_str);

unsigned int jenkins(const void *_str);

unsigned int RSHash(const void *_str, unsigned int len);

unsigned int JSHash(const void *_str, unsigned int len);

unsigned int PJWHash(const void *_str, unsigned int len);

unsigned int SDBMHash(const void *_str, unsigned int len);

unsigned int DJBHash(const void *_str, unsigned int len);

unsigned int DEKHash(const void *_str, unsigned int len);

unsigned int FNVHash(const void *_str, unsigned int len);

#endif /* CORE_HASHFUNCTIONS_H_ */
