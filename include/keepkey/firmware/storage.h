/*
 * This file is part of the TREZOR project.
 *
 * Copyright (C) 2014 Pavol Rusnak <stick@satoshilabs.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef STORAGE_H
#define STORAGE_H

#include "keepkey/crypto/bip32.h"
#include "keepkey/board/memory.h"

#include <stdbool.h>
#include <inttypes.h>

#define STORAGE_VERSION 7 /* Must add case fallthrough in storage_from_flash after increment*/
#define STORAGE_RETRIES 3

typedef struct _HDNode HDNode;
typedef struct _HDNodeType HDNodeType;
typedef struct _LoadDevice LoadDevice;
typedef struct _PolicyType PolicyType;
typedef struct _Storage Storage;

void storage_init(void);
void storage_reset_uuid(void);
void storage_reset(void);
void session_clear(bool clear_pin);
void storage_commit(void);

void storage_load_device(LoadDevice *msg);

const uint8_t *storage_getSeed(bool usePassphrase);
bool storage_get_root_node(HDNode *node, const char *curve, bool usePassphrase);

void storage_set_label(const char *label);
const char *storage_get_label(void);

void storage_set_language(const char *lang);
const char *storage_get_language(void);

bool storage_is_pin_correct(const char *pin);
bool storage_has_pin(void);
void storage_set_pin(const char *pin);
const char *storage_get_pin(void);
void session_cache_pin(const char *pin);
bool session_is_pin_cached(void);

/// Find the first nonzero entry in the pin_fail_arena.
///
/// \param pin_fail_arena   The arena to search through.
/// \param len              The number of arena entries.
///
/// \returns a pointer to the entry if one exists. Otherwise returns nullptr.
uint32_t *storage_getPinArenaElement(uint32_t *pin_fail_arena, size_t len);

/// Count the number of 0 bits in a PinFailArena element, which is indicative
/// of the amount of time needed for pin failure delay.
///
/// \param arena_elt    Pointer to the arena element to grab the count from.
/// \returns            The number of pin failures.
uint32_t storage_getPinArenaFailCount(uint32_t *arena_elt);

/// Reset the pin fail arena so that it contains as much 1 bits as possible.
///
/// `storage_getPinArenaFailCount()` must be invariant across calls to this function.
void storage_resetPinArena(uint32_t *pin_fail_arena, size_t len);

void storage_reset_pin_fails(void);
void storage_increase_pin_fails(void);
uint32_t storage_get_pin_fails(void);

bool storage_is_initialized(void);

const char *storage_get_uuid_str(void);

bool storage_get_passphrase_protected(void);
void storage_set_passphrase_protected(bool passphrase);
void session_cache_passphrase(const char *passphrase);
bool session_is_passphrase_cached(void);

void storage_set_mnemonic_from_words(const char (*words)[12], unsigned int num_words);
void storage_set_mnemonic(const char *mnemonic);
bool storage_has_mnemonic(void);

const char *storage_get_mnemonic(void);
const char *storage_get_shadow_mnemonic(void);

bool storage_get_imported(void);

bool storage_has_node(void);
HDNodeType *storage_get_node(void);

Allocation get_storage_location(void);

bool storage_set_policy(PolicyType *policy);
void storage_get_policies(PolicyType *policies);
bool storage_is_policy_enabled(char *policy_name);

#endif