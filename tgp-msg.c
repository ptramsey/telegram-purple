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

#include "telegram-purple.h"

#include <assert.h>
#include <tgl.h>
#include <glib.h>
#include <errno.h>
#include <locale.h>
#include <tgl-queries.h>

#include "telegram-base.h"
#include "tgp-structs.h"
#include "tgp-msg.h"
#include "tgp-ft.h"
#include "tgp-2prpl.h"
#include "tgp-chat.h"
#include "tgp-utils.h"
#include "tgp-chat.h"
#include "msglog.h"

static char *format_service_msg (struct tgl_state *TLS, struct tgl_message *M) {
  assert (M && M->flags & TGLMF_SERVICE);
  connection_data *conn = TLS->ev_base;
  char *txt = NULL;
  
  tgl_peer_t *fromPeer = tgl_peer_get (TLS, M->from_id);
  if (! fromPeer) {
    return NULL;
  }
  char *txt_user = fromPeer->print_name;

  switch (M->action.type) {
    case tgl_message_action_chat_create:
      txt = g_strdup_printf (_("%2$s created chat %1$s."), M->action.title, txt_user);
      break;
    case tgl_message_action_chat_edit_title:
      txt = g_strdup_printf (_("%2$s changed title to %1$s."), M->action.new_title, txt_user);
      break;
    case tgl_message_action_chat_edit_photo:
      txt = g_strdup_printf (_("%s changed photo."), txt_user);
      break;
    case tgl_message_action_chat_delete_photo:
      txt = g_strdup_printf (_("%s deleted photo."), txt_user);
      break;
    case tgl_message_action_chat_add_user_by_link: {
      tgl_peer_t *actionPeer = tgl_peer_get (TLS, TGL_MK_USER (M->action.user));
      if (actionPeer) {
        char *alias = actionPeer->print_name;
        
        PurpleConversation *conv = purple_find_chat (conn->gc, tgl_get_peer_id (M->to_id));
        txt = g_strdup_printf (_("%1$s added user %2$s by link."), alias, txt_user);
        if (conv) {
          p2tgl_conv_add_user (TLS, conv, tgl_get_peer_id (fromPeer->id), NULL, 0, FALSE);
        }

        return txt;
      }
      break;
    }
    case tgl_message_action_chat_add_user: {
      tgl_peer_t *peer = tgl_peer_get (TLS, TGL_MK_USER (M->action.user));
      if (peer) {
        char *alias = peer->print_name;
        txt = g_strdup_printf (_("%2$s added user %1$s."), alias, txt_user);
        PurpleConversation *conv = purple_find_chat (conn->gc, tgl_get_peer_id (M->to_id));
        if (conv) {
          p2tgl_conv_add_user (TLS, conv, M->action.user, NULL, 0, FALSE);
        }
      }
      break;
    }
    case tgl_message_action_chat_delete_user: {
      tgl_peer_t *peer = tgl_peer_get (TLS, TGL_MK_USER (M->action.user));
      if (peer) {

        tgl_peer_t *chatPeer = tgl_peer_get (TLS, M->to_id);
        assert (tgl_get_peer_type(chatPeer->id) == TGL_PEER_CHAT);
        
        // make sure that the chat is showing before deleting the user, otherwise the
        // chat will be initialised after removing the chat and the chat will still contain
        // the deleted user
        PurpleConversation *conv = tgp_chat_show (TLS, &chatPeer->chat);
        if (conv) {
          char *alias = peer->print_name;
          const char *aliasLeft = tgp_blist_peer_get_purple_name (TLS, TGL_MK_USER (M->action.user));
          
          txt = g_strdup_printf (_("%1$s deleted user %2$s."), txt_user, alias);
          
          g_return_val_if_fail (aliasLeft, txt);
          
          purple_conv_chat_remove_user (purple_conversation_get_chat_data (conv), aliasLeft, txt);
          if (M->action.user == tgl_get_peer_id (TLS->our_id)) {
            purple_conv_chat_left (purple_conversation_get_chat_data (conv));
          }
          
          // conv_del_user already printed a message, prevent a redundant message
          // from being printed by returning NULL
          return NULL;
        }
        
        char *alias = peer->print_name;
        txt = g_strdup_printf (_("%2$s deleted user %1$s."), alias, txt_user);
      }
      break;
    }
    case tgl_message_action_set_message_ttl:
      txt = g_strdup_printf (_("%2$s set self destruction timer to %1$d seconds."), M->action.ttl, txt_user);
      break;
    case tgl_message_action_read_messages:
      txt = g_strdup_printf (_("%2$s marked %1$d messages read."), M->action.read_cnt, txt_user);
      break;
    case tgl_message_action_delete_messages:
      txt = g_strdup_printf (_("%2$s deleted %1$d messages."), M->action.delete_cnt, txt_user);
      break;
    case tgl_message_action_screenshot_messages:
      txt = g_strdup_printf (_("%2$s made a screenshot of %1$d messages."), M->action.screenshot_cnt, txt_user);
      break;
    default:
      break;
  }
  return txt;
}

static char *format_geo_link_osm (double lat, double lon) {
  // assure that the floats are formatted with a dot
  char *loc = setlocale (LC_NUMERIC, NULL);
  setlocale (LC_NUMERIC, "en_US");
  debug ("old locale: %s", loc);

  char *link = g_strdup_printf ("https://www.openstreetmap.org/?mlat=%.6lf&mlon=%.6lf#map=17/%.6lf/%.6lf",
          lat, lon, lat, lon);

  // restore old locale
  setlocale (LC_NUMERIC, loc);
  return link;
}

static void tgp_msg_send_done (struct tgl_state *TLS, void *callback_extra, int success, struct tgl_message *M) {
  if (! success) {
    char *err = g_strdup_printf(_("Sending message failed. %d: %s"), TLS->error_code, TLS->error);
    warning (err);
    if (M) {
      tgp_msg_err_out (TLS, err, M->to_id);
    } else {
      fatal (err);
    }
    g_free (err);
    return;
  }
  
  write_files_schedule (TLS);
}

static gboolean tgp_msg_send_schedule_cb (gpointer data) {
  connection_data *conn = data;
  conn->out_timer = 0;
  struct tgp_msg_sending *D = NULL;

  while ((D = g_queue_peek_head (conn->out_messages))) {
    g_queue_pop_head (conn->out_messages);

    // TODO: option for disable_msg_preview
    tgl_do_send_message (D->TLS, D->to, D->msg, (int)strlen (D->msg), 0, NULL, tgp_msg_send_done, NULL);
    tgp_msg_sending_free (D);
  }
  return FALSE;
}

static void tgp_msg_send_schedule (struct tgl_state *TLS, gchar *chunk, tgl_peer_id_t to) {
  connection_data *conn = TLS->ev_base;
  struct tgp_msg_sending *D = tgp_msg_sending_init (TLS, chunk, to);
  g_queue_push_tail (conn->out_messages, D);

  if (conn->out_timer) {
    purple_timeout_remove (conn->out_timer);
  }
  conn->out_timer = purple_timeout_add (0, tgp_msg_send_schedule_cb, conn);
}

static int tgp_msg_send_split (struct tgl_state *TLS, const char *message, tgl_peer_id_t to) {
  int max = TGP_DEFAULT_MAX_MSG_SPLIT_COUNT;
  if (max < 1) {
    max = 1;
  }
  int size = (int)g_utf8_strlen(message, -1);
  if (size > TGP_MAX_MSG_SIZE * max) {
    return -E2BIG;
  }
  int start = 0;
  while (size > start) {
    int e = start + (int)TGP_MAX_MSG_SIZE;
    gchar *chunk = g_utf8_substring (message, start, e);
    tgp_msg_send_schedule (TLS, chunk, to);
    start = e;
  }
  return 1;
}

void tgp_msg_err_out (struct tgl_state *TLS, const char *error, tgl_peer_id_t to) {
  int flags = PURPLE_MESSAGE_ERROR | PURPLE_MESSAGE_SYSTEM;
  time_t now;
  time (&now);
  switch (tgl_get_peer_type (to)) {
      case TGL_PEER_CHAT:
        p2tgl_got_chat_in (TLS, to, to, error, flags, now);
        break;
      case TGL_PEER_USER:
      case TGL_PEER_ENCR_CHAT:
        serv_got_im (tg_get_conn (TLS), tgp_blist_peer_get_purple_name (TLS, to), error, flags, now);
        break;
  }
}

void tgp_msg_sys_out (struct tgl_state *TLS, const char *msg, tgl_peer_id_t to_id, int no_log) {
  int flags = PURPLE_MESSAGE_SYSTEM;
  if (no_log) {
    flags |= PURPLE_MESSAGE_NO_LOG;
  }
  time_t now;
  time (&now);
  
  switch (tgl_get_peer_type (to_id)) {
    case TGL_PEER_CHAT:
      p2tgl_got_chat_in (TLS, to_id, to_id, msg, flags, now);
      break;
    case TGL_PEER_USER:
    case TGL_PEER_ENCR_CHAT: {
      const char *name = tgp_blist_peer_get_purple_name (TLS, to_id);
      PurpleConversation *conv = p2tgl_find_conversation_with_account (TLS, to_id);
      
      g_return_if_fail (name);

      if (! conv) {
        conv = purple_conversation_new (PURPLE_CONV_TYPE_IM, tg_get_acc (TLS), name);
      }
      purple_conversation_write (conv, name, msg, flags, now);
      break;
    }
  }
}

void send_inline_picture_done (struct tgl_state *TLS, void *extra, int success, struct tgl_message *msg) {
  if (!success) {
    char *errormsg = g_strdup_printf ("%d: %s", TLS->error_code, TLS->error);
    failure (errormsg);
    purple_notify_message (_telegram_protocol, PURPLE_NOTIFY_MSG_ERROR, _("Sending image failed."),
                           errormsg, NULL, NULL, NULL);
    g_free (errormsg);
    return;
  }
}

int tgp_msg_send (struct tgl_state *TLS, const char *message, tgl_peer_id_t to) {
  // search for outgoing embedded image tags and send them
  gchar *img = NULL;
  gchar *stripped = NULL;
  
  if ((img = g_strrstr (message, "<IMG")) || (img = g_strrstr (message, "<img"))) {
    if (tgl_get_peer_type(to) == TGL_PEER_ENCR_CHAT) {
      tgp_msg_err_out (TLS, _("Sorry, sending documents to encrypted chats not yet supported."), to);
      return 0;
    }
    debug ("img found: %s", img);
    gchar *id;
    if ((id = g_strrstr (img, "ID=\"")) || (id = g_strrstr (img, "id=\""))) {
      id += 4;
      int imgid = atoi (id);
      if (imgid > 0) {
        PurpleStoredImage *psi = purple_imgstore_find_by_id (imgid);
        gchar *tmp = g_build_filename (g_get_tmp_dir(), purple_imgstore_get_filename (psi), NULL) ;
        GError *err = NULL;
        gconstpointer data = purple_imgstore_get_data (psi);
        g_file_set_contents (tmp, data, purple_imgstore_get_size (psi), &err);
        if (! err) {
          stripped = g_strstrip(purple_markup_strip_html (message));
          tgl_do_send_document (TLS, to, tmp, stripped, (int)strlen (stripped),
                                TGL_SEND_MSG_FLAG_DOCUMENT_AUTO, send_inline_picture_done, NULL);
          g_free (stripped);
          
          // return 0 to assure that the picture is not echoed, since
          // it will already be echoed with the outgoing message
          return 0;
        } else {
          failure ("Storing %s in imagestore failed: %s\n", tmp, err->message);
          g_error_free (err);
          return -1;
        }
      }
    }
    // no image id found in image
    return -1;
  }
  
#ifndef __ADIUM_
  /*
    Adium won't escape any HTML markup and just pass any user-input through,
    while Pidgin will replace special chars with the escape chars and also add 
    additional markup for RTL languages and such.

    First, we remove any HTML markup added by Pidgin, since Telegram won't handle it properly.
    User-entered HTML is still escaped and therefore won't be harmed.
   */
  stripped = purple_markup_strip_html (message);
  
  /* 
    now unescape the markup, so that html special chars will still show
    up properly in Telegram
   */
  gchar *unescaped = purple_unescape_text (stripped);
  int ret = tgp_msg_send_split (TLS, stripped, to);
  
  g_free (unescaped);
  g_free (stripped);
  return ret;
#endif
  
  return tgp_msg_send_split (TLS, message, to);
}

static char *tgp_msg_photo_display (struct tgl_state *TLS, tgl_peer_id_t from, const char *filename, int *flags) {
  char *text = NULL;

#ifdef ENABLE_INLINE_IMAGES
  connection_data *conn = TLS->ev_base;
  int img = p2tgl_imgstore_add_with_id (filename);
  if (img <= 0) {
    failure ("Cannot display picture, adding to imgstore failed.");
    return NULL;
  }
  used_images_add (conn, img);
  *flags |= PURPLE_MESSAGE_IMAGES;
  text = tgp_format_img (img);
#else
  const char *txt_user = tgp_blist_peer_get_purple_name (TLS, from);

  g_return_val_if_fail (txt_user, NULL);

  text = g_strdup_printf (_("%s sent a photo."), txt_user);
  *flags |= PURPLE_MESSAGE_SYSTEM;
#endif

  return text;
}

static char *tgp_msg_sticker_display (struct tgl_state *TLS, tgl_peer_id_t from, const char *filename, int *flags) {
  char *text = NULL;
  
#if defined(HAVE_LIBWEBP) && defined(ENABLE_INLINE_IMAGES)
  connection_data *conn = TLS->ev_base;
  int img = p2tgl_imgstore_add_with_id_webp ((char *) filename);
  if (img <= 0) {
    failure ("Cannot display sticker, adding to imgstore failed");
    return NULL;
  }
  used_images_add (conn, img);
  text = tgp_format_img (img);
  *flags |= PURPLE_MESSAGE_IMAGES;
#else
  const char *txt_user = tgp_blist_peer_get_purple_name (TLS, from);
  
  g_return_val_if_fail (txt_user, NULL);
  
  text = g_strdup_printf (_("%s sent a sticker."), txt_user);
  *flags |= PURPLE_MESSAGE_SYSTEM;
#endif
  return text;
}

static void tgp_msg_display (struct tgl_state *TLS, struct tgp_msg_loading *C) {
  connection_data *conn = TLS->ev_base;
  struct tgl_message *M = C->msg;
  char *text = NULL;
  int flags = 0;
  
  if (M->flags & (TGLMF_EMPTY | TGLMF_DELETED)) {
    return;
  }
  if (!(M->flags & TGLMF_CREATED)) {
    return;
  }
  if (!tgl_get_peer_type (M->to_id)) {
    warning ("Bad msg\n");
    return;
  }

  // only display new messages, ignore updates or deletions
  if (!M->message || tgp_outgoing_msg (TLS, M) || !tgl_get_peer_type (M->to_id)) {
    return;
  }
  
  // Mark messages that contain a mention like if they contained our current nick name
  if (M->flags & TGLMF_MENTION) {
    flags |= PURPLE_MESSAGE_NICK;
  }

  // format the message text
  if (M->flags & TGLMF_SERVICE) {
    text = format_service_msg (TLS, M);
    flags |= PURPLE_MESSAGE_SYSTEM;
    
  } else if (M->media.type != tgl_message_media_none) {
    switch (M->media.type) {
  
      case tgl_message_media_photo: {
        if (M->media.photo) {
          assert (C->data);
          text = tgp_msg_photo_display (TLS, M->from_id, C->data, &flags);
          if (str_not_empty (text)) {
            if (str_not_empty (M->media.caption)) {
              char *old = text;
              text = g_strdup_printf ("%s<br>%s", old, M->media.caption);
              g_free (old);
            }
          }
        }
        break;
      }
        
      case tgl_message_media_document:
        if (M->media.document->flags & TGLDF_STICKER) {
          assert (C->data);
          text = tgp_msg_sticker_display (TLS, M->from_id, C->data, &flags);
        } else if (M->media.document->flags & TGLDF_IMAGE) {
          assert (C->data);
          text = tgp_msg_photo_display (TLS, M->from_id, C->data, &flags);
        } else {
          if (! tgp_our_msg(TLS, M)) {
            tgprpl_recv_file (conn->gc, tgp_blist_peer_get_purple_name (TLS, M->from_id), M);
          }
          return;
        }
        break;
        
      case tgl_message_media_video:
      case tgl_message_media_audio: {
        if (! tgp_our_msg(TLS, M)) {
          tgprpl_recv_file (conn->gc, tgp_blist_peer_get_purple_name (TLS, M->from_id), M);
        }
      }
      break;
        
      case tgl_message_media_document_encr:
        if (M->media.encr_document->flags & TGLDF_STICKER) {
          assert (C->data);
          text = tgp_msg_sticker_display (TLS, M->from_id, C->data, &flags);
        } if (M->media.encr_document->flags & TGLDF_IMAGE) {
          assert (C->data);
          text = tgp_msg_photo_display (TLS, M->from_id, C->data, &flags);
        } else {
          if (! tgp_our_msg(TLS, M)) {
            tgprpl_recv_file (conn->gc, tgp_blist_peer_get_purple_name (TLS, M->to_id), M);
          }
          return;
        }
        break;
      
      case tgl_message_media_contact:
        text = g_strdup_printf ("<b>%s %s</b> %s", M->media.first_name, M->media.last_name, M->media.phone);
        break;
        
      case tgl_message_media_venue: {
        char *address = NULL;
        if (M->media.venue.address && strcmp (M->media.venue.title, M->media.venue.address)) {
          address = g_strdup_printf (" %s", M->media.venue.address);
        }
        char *pos = format_geo_link_osm (M->media.venue.geo.latitude, M->media.geo.longitude);
        text = g_strdup_printf ("<a href=\"%s\">%s</a>%s",
                                pos,
                                M->media.venue.title ? M->media.venue.title : "", address ? address : "");
        if (address) {
          g_free (address);
        }
        g_free (pos);
        break;
      }
        
      case tgl_message_media_geo: {
        char *pos = format_geo_link_osm (M->media.venue.geo.latitude, M->media.geo.longitude);
        text = g_strdup_printf ("<a href=\"%s\">%s</a>", pos, pos);
        g_free (pos);
        break;
      }
        
      case tgl_message_media_webpage: {
        char *msg = g_strdup (M->message);
        text = purple_markup_escape_text (msg, strlen (msg));
        g_free (msg);
        break;
      }
        
      default:
        warning ("received unknown media type: %d", M->media.type);
        break;
    }
    
  } else {
    if (str_not_empty (M->message)) {
      text = purple_markup_escape_text (M->message, strlen (M->message));
    }
    flags |= PURPLE_MESSAGE_RECV;
  }
  
  if (tgl_get_peer_type (M->to_id) != TGL_PEER_ENCR_CHAT && ! (M->flags & TGLMF_UNREAD)) {
    flags |= PURPLE_MESSAGE_DELAYED;
  }
  
  // some service messages (like removing/adding users from chats) might print the message 
  // text through other means and leave the text empty
  if (! str_not_empty (text)) {
    return;
  }
  
  // display the message to the user
  switch (tgl_get_peer_type (M->to_id)) {
    case TGL_PEER_CHAT: {
      tgl_peer_t *P = tgl_peer_get (TLS, M->to_id);
      if (tgp_chat_show (TLS, &P->chat)) {
        p2tgl_got_chat_in (TLS, M->to_id, M->from_id, text, flags, M->date);
      }
      pending_reads_add (conn->pending_reads, M->to_id);
      break;
    }
    case TGL_PEER_ENCR_CHAT: {
      p2tgl_got_im_combo (TLS, M->to_id, text, flags, M->date);
      pending_reads_add (conn->pending_reads, M->to_id);
      break;
    }
    case TGL_PEER_USER: {
      if (tgp_our_msg (TLS, M)) {
        flags |= PURPLE_MESSAGE_SEND;
        flags &= ~PURPLE_MESSAGE_RECV;
        p2tgl_got_im_combo (TLS, M->to_id, text, flags, M->date);
      } else {
        p2tgl_got_im_combo (TLS, M->from_id, text, flags, M->date);
        pending_reads_add (conn->pending_reads, M->from_id);
      }
      break;
    }
  }
  
  g_free (text);
}

static time_t tgp_msg_oldest_relevant_ts (struct tgl_state *TLS) {
  connection_data *conn = TLS->ev_base;
  int days = purple_account_get_int (conn->pa, TGP_KEY_HISTORY_RETRIEVAL_THRESHOLD,
                                     TGP_DEFAULT_HISTORY_RETRIEVAL_THRESHOLD);
  return days > 0 ? tgp_time_n_days_ago (days) : 0;
}

static void tgp_msg_process_in_ready (struct tgl_state *TLS) {
  connection_data *conn = TLS->ev_base;
  struct tgp_msg_loading *C;
  
  while ((C = g_queue_peek_head (conn->new_messages))) {
    if (C->pending) {
      break;
    }
    g_queue_pop_head (conn->new_messages);
    
    pending_reads_send_all (TLS);
    tgp_msg_display (TLS, C);
    if (C->data) {
      g_free (C->data);
    }
    tgp_msg_loading_free (C);
  }
}

static void tgp_msg_on_loaded_document (struct tgl_state *TLS, void *extra, int success, const char *filename) {
  debug ("tgp_msg_on_loaded_document()");
  assert (success);
  
  struct tgp_msg_loading *C = extra;
  C->data = (void *) g_strdup (filename);
  -- C->pending;
  tgp_msg_process_in_ready (TLS);
}

static void tgp_msg_on_loaded_chat_full (struct tgl_state *TLS, void *extra, int success, struct tgl_chat *chat) {
  debug ("tgp_msg_on_loaded_chat_full()");
  assert (success);
  
  tgp_chat_on_loaded_chat_full (TLS, chat);
  
  struct tgp_msg_loading *C = extra;
  -- C->pending;
  tgp_msg_process_in_ready (TLS);
}

void tgp_msg_recv (struct tgl_state *TLS, struct tgl_message *M) {
  connection_data *conn = TLS->ev_base;
  if (M->flags & (TGLMF_EMPTY | TGLMF_DELETED)) {
    return;
  }
  if (!(M->flags & TGLMF_CREATED)) {
    return;
  }
  if (!(M->flags | TGLMF_UNREAD) && M->date != 0 && M->date < tgp_msg_oldest_relevant_ts (TLS)) {
    debug ("Message from %d on %d too old, ignored.", tgl_get_peer_id (M->from_id), M->date);
    return;
  }
  
  struct tgp_msg_loading *C = tgp_msg_loading_init (M);
  
  if (! (M->flags & TGLMF_SERVICE)) {
    
    // handle all messages that need to load content before they can be displayed
    if (M->media.type != tgl_message_media_none) {
      switch (M->media.type) {
        case tgl_message_media_photo: {
          
          // include the "bad photo" check from telegram-cli interface.c:3287 to avoid crashes when fetching history
          // TODO: find out the reason for this behavior
          if (M->media.photo) {
            ++ C->pending;
            tgl_do_load_photo (TLS, M->media.photo, tgp_msg_on_loaded_document, C);
          }
          break;
        }
          
        // documents that are stickers or images will be displayed just like regular photo messages
        // and need to be lodaed beforehand
        case tgl_message_media_document:
        case tgl_message_media_video:
        case tgl_message_media_audio:
          if (M->media.document->flags & TGLDF_STICKER || M->media.document->flags & TGLDF_IMAGE) {
            ++ C->pending;
            tgl_do_load_document (TLS, M->media.document, tgp_msg_on_loaded_document, C);
          }
          break;
        case tgl_message_media_document_encr:
          if (M->media.encr_document->flags & TGLDF_STICKER || M->media.encr_document->flags & TGLDF_IMAGE) {
            ++ C->pending;
            tgl_do_load_encr_document (TLS, M->media.encr_document, tgp_msg_on_loaded_document, C);
          }
          break;
          
        case tgl_message_media_geo:
          // TODO: load geo thumbnail
          break;
          
        default: // prevent Clang warnings ...
          break;
      }
    }
  }
  
  if (tgl_get_peer_type (M->to_id) == TGL_PEER_CHAT) {
      
    tgl_peer_t *peer = tgl_peer_get (TLS, M->to_id);
    assert (peer);
      
    if (! peer->chat.user_list_size) {
      // To display a chat the full name of every single user is needed, but the updates received from the server only
      // contain the names of users mentioned in the events. In order to display a messages we always need to fetch the
      // full chat info first. If the user list is empty, this means that we still haven't fetched the full chat information.
      
      // assure that there is only one chat info request for every
      // chat to avoid causing FLOOD_WAIT_X errors that will lead to delays or dropped messages
      gpointer to_ptr = GINT_TO_POINTER(tgl_get_peer_id (M->to_id));
      
      if (! g_hash_table_lookup (conn->pending_chat_info, to_ptr)) {
        ++ C->pending;
        
        tgl_do_get_chat_info (TLS, M->to_id, FALSE, tgp_msg_on_loaded_chat_full, C);
        g_hash_table_replace (conn->pending_chat_info, to_ptr, to_ptr);
      }
    }
  }
  
  g_queue_push_tail (conn->new_messages, C);
  tgp_msg_process_in_ready (TLS);
}

