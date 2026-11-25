#!/usr/bin/env python3
"""Make fake library updates for testing the screens without an account (no real people or chats).
python3 tools/demo.py > build/demo.json ; used through data/debug.json {"demo": "<path>", "open": <chat id>, "say": "text"}"""
import json, time
now = int(time.time()); ME = 100
def user(i, first, last="", status="userStatusRecently", bot=False):
    return {"@type": "updateUser", "user": {"@type": "user", "id": i, "first_name": first, "last_name": last, "status": {"@type": status, "was_online": now - 4000},
            "type": {"@type": "userTypeBot" if bot else "userTypeRegular"}}}
def text(s, ents=()): return {"@type": "messageText", "text": {"@type": "formattedText", "text": s, "entities": [
    {"@type": "textEntity", "offset": o, "length": l, "type": dict({"@type": "textEntityType" + t}, **x)} for o, l, t, x in ents]}}
def msg(chat, i, sender, content, ago, out=False, **more):
    m = {"@type": "message", "id": i << 20, "chat_id": chat, "is_outgoing": out, "date": now - ago, "edit_date": 0, "content": content,
         "sender_id": {"@type": "messageSenderUser", "user_id": ME if out else sender}}
    m.update(more); return m
def chat(i, title, ctype, order, last, unread=0, pinned=False, muted=False, archive=False, read_out=0):
    return {"@type": "updateNewChat", "chat": {"@type": "chat", "id": i, "title": title, "type": ctype, "unread_count": unread, "unread_mention_count": 0,
            "is_marked_as_unread": False, "last_read_outbox_message_id": read_out, "last_message": last,
            "permissions": {"@type": "chatPermissions", "can_send_basic_messages": True},
            "notification_settings": {"@type": "chatNotificationSettings", "use_default_mute_for": False, "mute_for": 9999 if muted else 0},
            "positions": [{"@type": "chatPosition", "list": {"@type": "chatListArchive" if archive else "chatListMain"}, "order": str(order), "is_pinned": pinned}]}}
priv = lambda u: {"@type": "chatTypePrivate", "user_id": u}
sg = lambda g, ch: {"@type": "chatTypeSupergroup", "supergroup_id": g, "is_channel": ch}
doc = {"@type": "messageDocument", "caption": {"@type": "formattedText", "text": "the plan"}, "document": {"@type": "document", "file_name": "plan.pdf",
       "document": {"@type": "file", "id": 71, "size": 1843200, "expected_size": 1843200, "local": {"@type": "localFile", "path": "", "can_be_downloaded": True}}}}
photo = {"@type": "messagePhoto", "caption": {"@type": "formattedText", "text": ""}, "photo": {"@type": "photo", "sizes": [
         {"@type": "photoSize", "type": "x", "width": 800, "height": 600, "photo": {"@type": "file", "id": 72, "size": 90000, "local": {"@type": "localFile", "path": "", "can_be_downloaded": False}}}]}}
voice = {"@type": "messageVoiceNote", "caption": {"@type": "formattedText", "text": ""}, "voice_note": {"@type": "voiceNote", "duration": 17,
         "voice": {"@type": "file", "id": 73, "size": 40100, "local": {"@type": "localFile", "path": "", "can_be_downloaded": True}}}}
h1 = [msg(1, 9, 1, text("See you at 7 then 👍"), 60, interaction_info={"@type": "messageInteractionInfo", "view_count": 0, "reactions": {"@type": "messageReactions",
          "reactions": [{"@type": "messageReaction", "type": {"@type": "reactionTypeEmoji", "emoji": "👍"}, "total_count": 2, "is_chosen": True}]}}, is_pinned=True),
      msg(1, 8, 1, voice, 200),
      msg(1, 7, 1, text("Sure, sending it now"), 400, out=True, reply_to={"@type": "messageReplyToMessage", "chat_id": 1, "message_id": 6 << 20}),
      msg(1, 6, 1, doc, 500),
      msg(1, 5, 1, photo, 3000),
      msg(1, 4, 1, text("Bold and a link: example.org & <tags>", [(0, 4, "Bold", {}), (17, 11, "Url", {})]), 3600, edit_date=now - 3500),
      msg(1, 3, 1, text("Morning!\nSecond line"), 90000, out=True),
      msg(1, 2, 1, text("Old message, forwarded"), 200000, forward_info={"@type": "messageForwardInfo", "origin": {"@type": "messageOriginHiddenUser", "sender_name": "Somebody"}}),
      msg(1, 1, 1, {"@type": "messageContactRegistered"}, 400000)]
def extra(m, k): m["id"] = (8 << 20) + k; return m
ft = lambda t: {"@type": "formattedText", "text": t, "entities": []}
opt = lambda t, pct, ch=False: {"@type": "pollOption", "text": ft(t), "vote_percentage": pct, "voter_count": pct // 10, "is_chosen": ch}
poll = {"@type": "messagePoll", "poll": {"@type": "poll", "id": "1", "question": ft("Where do we eat?"), "total_voter_count": 7, "is_anonymous": True,
        "allows_multiple_answers": False, "is_closed": False, "type": {"@type": "pollTypeRegular"},
        "options": [opt("Pizza place", 0), opt("Noodle bar", 0), opt("Cook at home", 0)]}}
poll2 = {"@type": "messagePoll", "poll": {"@type": "poll", "id": "2", "question": ft("Which days work?"), "total_voter_count": 10, "is_anonymous": False,
        "allows_multiple_answers": True, "is_closed": False, "type": {"@type": "pollTypeRegular"},
        "options": [opt("Friday", 60, True), opt("Saturday", 30), opt("Sunday", 10)]}}
link = text("Have a look: https://example.org/trail")
link["link_preview"] = {"@type": "linkPreview", "url": "https://example.org/trail", "site_name": "Example Trails", "title": "Lake loop, 12 km",
                        "description": ft("An easy walk around the lake with two places to stop for food."), "type": {"@type": "linkPreviewTypeUnsupported"}}
kb = {"@type": "replyMarkupInlineKeyboard", "rows": [
      [{"@type": "inlineKeyboardButton", "text": "Yes", "type": {"@type": "inlineKeyboardButtonTypeCallback", "data": "eWVz"}},
       {"@type": "inlineKeyboardButton", "text": "No", "type": {"@type": "inlineKeyboardButtonTypeCallback", "data": "bm8="}}],
      [{"@type": "inlineKeyboardButton", "text": "Copy the code", "type": {"@type": "inlineKeyboardButtonTypeCopyText", "text": "TRAIL12"}}]]}
import os
DATA = os.environ.get("DEMO_DATA", "")            # the app's data folder on the phone (animation test files are copied there)
def anim_sticker(fid, fname, fmt):
    f = {"@type": "file", "id": fid, "size": 6000, "local": {"@type": "localFile", "path": DATA + "/" + fname, "is_downloading_completed": True, "can_be_downloaded": True}}
    th = {"@type": "thumbnail", "format": {"@type": "thumbnailFormatWebp"}, "width": 128, "height": 128,
          "file": {"@type": "file", "id": fid + 1, "size": 100, "local": {"@type": "localFile", "path": "", "can_be_downloaded": False}}}
    return {"@type": "messageSticker", "sticker": {"@type": "sticker", "id": fid, "set_id": 1, "width": 512, "height": 512, "emoji": "🎉",
            "format": {"@type": fmt}, "thumbnail": th, "sticker": f}, "is_premium": False}
h1[1:1] = [extra(msg(1, 8, 1, anim_sticker(950, "demo-party.tgs", "stickerFormatTgs"), 70), 6),
           extra(msg(1, 8, 1, anim_sticker(960, "demo-test.webm", "stickerFormatWebm"), 75, out=True), 5)]
h1[3:3] = [extra(msg(1, 8, 1, poll, 80), 4), extra(msg(1, 8, 1, poll2, 90), 3), extra(msg(1, 8, 1, link, 120), 2),
           extra(msg(1, 8, 1, text("Coming on Saturday?"), 150, reply_markup=kb), 1)]
h2 = [msg(-20, 5, 3, text("Who brings the tent?"), 30), msg(-20, 4, 3, text("And I can drive"), 100), msg(-20, 3, 2, text("I am in"), 300),
      msg(-20, 2, 2, {"@type": "messageChatAddMembers", "member_user_ids": [3]}, 90000), msg(-20, 1, 1, text("Trip planning starts here"), 100000, out=True)]
ups = [{"@type": "updateOption", "name": "my_id", "value": {"@type": "optionValueInteger", "value": str(ME)}},
       user(ME, "Me"), user(1, "Anna", "Example", "userStatusOnline"), user(2, "Ben", "Sample"), user(3, "Cleo", "Test", "userStatusOffline"), user(4, "Helper Bot", bot=True),
       {"@type": "updateSupergroup", "supergroup": {"@type": "supergroup", "id": 20, "member_count": 4, "is_channel": False, "status": {"@type": "chatMemberStatusMember"}}},
       {"@type": "updateSupergroup", "supergroup": {"@type": "supergroup", "id": 30, "member_count": 12345, "is_channel": True, "status": {"@type": "chatMemberStatusMember"}}},
       chat(1, "Anna Example", priv(1), 900, h1[0], unread=2, read_out=3 << 20),
       chat(-20, "Trip planning", sg(20, False), 800, h2[0], unread=128, muted=True),
       chat(-30, "News channel", sg(30, True), 700, msg(-30, 1, 0, text("Weekly summary is out"), 86400 * 3)),
       chat(ME, "Me", priv(ME), 2000, msg(ME, 1, ME, text("note to self"), 86400 * 40, out=True), pinned=True),
       chat(4, "Helper Bot", priv(4), 600, msg(4, 1, 4, text("/start"), 86400 * 400, out=True), read_out=1 << 20),
       chat(2, "Ben Sample", priv(2), 500, msg(2, 1, 2, photo, 7200), archive=True),
       {"@type": "updateSecretChat", "secret_chat": {"@type": "secretChat", "id": 77, "user_id": 3, "state": {"@type": "secretChatStatePending"}, "is_outbound": True}},
       chat(-77, "Cleo Test", {"@type": "chatTypeSecret", "secret_chat_id": 77, "user_id": 3}, 450, None),
       {"@type": "updateChatFolders", "main_chat_list_position": 0, "are_tags_enabled": False, "chat_folders": [
           {"@type": "chatFolderInfo", "id": 2, "name": {"@type": "chatFolderName", "text": {"@type": "formattedText", "text": "Friends", "entities": []}}},
           {"@type": "chatFolderInfo", "id": 3, "name": {"@type": "chatFolderName", "text": {"@type": "formattedText", "text": "Work", "entities": []}}}]},
       {"@type": "updateChatPosition", "chat_id": 1, "position": {"@type": "chatPosition", "list": {"@type": "chatListFolder", "chat_folder_id": 2}, "order": "900", "is_pinned": False}},
       {"@type": "updateUnreadMessageCount", "chat_list": {"@type": "chatListMain"}, "unread_count": 130, "unread_unmuted_count": 2}]
print(json.dumps({"updates": ups, "history": {"1": h1, "-20": h2}}))
