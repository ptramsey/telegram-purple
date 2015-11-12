/*
    This file is part of telegram-purple

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA

    Copyright Matthias Jentsch 2014-2015
*/
#ifndef __telegram_adium__purple2tgl__
#define __telegram_adium__purple2tgl__

#include <stdio.h>

#include "telegram-purple.h"
#include "account.h"
#include "conversation.h"
#include "prpl.h"

PurpleAccount *tg_get_acc (struct tgl_state *TLS);
PurpleConnection *tg_get_conn (struct tgl_state *TLS);

connection_data *tg_get_data (struct tgl_state *TLS);
connection_data *gc_get_conn (PurpleConnection *gc);
connection_data *pa_get_conn (PurpleAccount *pa);
connection_data *pbn_get_conn (PurpleBlistNode *node);
connection_data *c_get_conn (struct connection *c);

tgl_peer_t *p2tgl_get_peer (tgl_peer_id_t peer);
tgl_peer_t *p2tgl_get_peer_by_id (int id);

int p2tgl_status_is_present (PurpleStatus *status);
int p2tgl_send_notifications (PurpleAccount *acct);

void p2tgl_got_chat_invite (PurpleConnection *gc, tgl_peer_t *chat, tgl_peer_id_t inviter, const char *message);
void p2tgl_got_chat_left (struct tgl_state *TLS, tgl_peer_id_t chat);
void p2tgl_got_chat_in (struct tgl_state *TLS, tgl_peer_id_t chat, tgl_peer_id_t who, const char *message, int flags, time_t when);
void p2tgl_got_im_combo (struct tgl_state *TLS, tgl_peer_id_t who, const char *msg, int flags, time_t when);

tgl_chat_id_t p2tgl_chat_get_id (PurpleChat *PC);
void p2tgl_buddy_add_data (struct tgl_state *TLS, tgl_peer_id_t user, void *data);
void p2tgl_prpl_got_user_status (struct tgl_state *TLS, tgl_peer_id_t user, struct tgl_user_status *status);

void p2tgl_conv_add_users (PurpleConversation *conv, struct tgl_chat_user *list);
void p2tgl_conv_add_user (struct tgl_state *TLS, PurpleConversation *conv, int user, char *message, int flags, int new_arrival);
PurpleConversation *p2tgl_find_conversation_with_account (struct tgl_state *TLS, tgl_peer_id_t peer);

PurpleNotifyUserInfo *p2tgl_notify_peer_info_new (struct tgl_state *TLS, tgl_peer_t *P);
PurpleNotifyUserInfo *p2tgl_notify_user_info_new (struct tgl_user *U);
PurpleNotifyUserInfo *p2tgl_notify_encrypted_chat_info_new (struct tgl_state *TLS, struct tgl_secret_chat *secret, struct tgl_user *U);

int p2tgl_imgstore_add_with_id (const char* filename);

void p2tgl_buddy_icons_set_for_user (PurpleAccount *pa, tgl_peer_id_t id, const char* filename);

#ifdef HAVE_LIBWEBP
int p2tgl_imgstore_add_with_id_webp (const char *filename);
#endif

#endif
