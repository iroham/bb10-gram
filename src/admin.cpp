// Store, part: group and channel admin tools. Promote with chosen rights, dismiss, restrict with permissions (for
// a time), ban/remove, unban, the group's default permissions and slow mode, title/description/photo, removed
// users and recent actions (the admin log). Answers come back tagged "adm:<what>" (Store::onAdminAnswer).
#include "store.h"
#include "tdclient.h"
#include <QDateTime>
#include <QStringList>
#include <QDebug>

typedef QVariantMap M;
static inline qint64 I(const QVariant &v) { return v.toLongLong(); }
static inline QString T(const M &m) { return m.value("@type").toString(); }
static inline M sub(const M &m, const char *k) { return m.value(k).toMap(); }
static M req(const char *type) { M m; m["@type"] = QString::fromLatin1(type); return m; }
static M user(const QVariant &id) { M w = req("messageSenderUser"); w["user_id"] = I(id); return w; }

static const char *const kRights[] = { "can_manage_chat", "can_change_info", "can_post_messages", "can_edit_messages", "can_delete_messages",
    "can_invite_users", "can_restrict_members", "can_pin_messages", "can_manage_topics", "can_promote_members", "can_manage_video_chats",
    "can_post_stories", "can_edit_stories", "can_delete_stories", "can_manage_direct_messages", "can_manage_tags", "can_send_welcome_messages",
    "is_anonymous", 0 };
static const char *const kPerms[] = { "can_send_basic_messages", "can_send_audios", "can_send_documents", "can_send_photos", "can_send_videos",
    "can_send_video_notes", "can_send_voice_notes", "can_send_polls", "can_send_other_messages", "can_add_link_previews",
    "can_react_to_messages", "can_edit_tag", "can_change_info", "can_invite_users", "can_pin_messages", "can_create_topics", 0 };

// a full object of the given type: the keys the page sent, the rest from `base`
static M fill(const char *type, const char *const *keys, const M &picked, const M &base) {
    M o = req(type);
    for (int i = 0; keys[i]; i++) o[keys[i]] = picked.contains(keys[i]) ? picked.value(keys[i]).toBool() : base.value(keys[i]).toBool();
    return o;
}

QVariantMap Store::memberInfo(const QVariant &userId) {
    M out; out["role"] = "member";
    QVariantList members = m_profileFull.value("members").toList();
    for (int i = 0; i < members.size(); i++) {
        M mm = members.at(i).toMap();
        if (I(sub(mm, "member_id").value("user_id")) != I(userId)) continue;
        M st = sub(mm, "status"); QString t = T(st);
        out["role"] = t == "chatMemberStatusCreator" ? "owner" : t == "chatMemberStatusAdministrator" ? "admin"
                    : t == "chatMemberStatusRestricted" ? "restricted" : t == "chatMemberStatusBanned" ? "banned" : "member";
        out["rights"] = sub(st, "rights");
        out["canBeEdited"] = st.value("can_be_edited").toBool();
        out["permissions"] = t == "chatMemberStatusRestricted" ? sub(st, "permissions") : sub(m_chats.value(m_profileId), "permissions");
        out["until"] = st.value("restricted_until_date").toInt();
        break;
    }
    if (!out.contains("permissions")) out["permissions"] = sub(m_chats.value(m_profileId), "permissions");
    out["name"] = userName(I(userId));
    return out;
}

void Store::promote(const QVariant &chatId, const QVariant &userId, const QVariantMap &rights) {
    M st = req("chatMemberStatusAdministrator");
    st["can_be_edited"] = true;
    M base = memberInfo(userId).value("rights").toMap();
    M r = fill("chatAdministratorRights", kRights, rights, base);
    r["can_manage_chat"] = true;                             // every admin has it
    st["rights"] = r;
    M q = req("setChatMemberStatus"); q["chat_id"] = I(chatId); q["member_id"] = user(userId); q["status"] = st;
    send(q, "adm:promoted");
}

void Store::demote(const QVariant &chatId, const QVariant &userId) {
    M st = req("chatMemberStatusMember"); st["member_until_date"] = 0;
    M q = req("setChatMemberStatus"); q["chat_id"] = I(chatId); q["member_id"] = user(userId); q["status"] = st;
    send(q, "adm:demoted");
}

void Store::restrictMember(const QVariant &chatId, const QVariant &userId, const QVariantMap &perms, int seconds) {
    M st = req("chatMemberStatusRestricted");
    st["is_member"] = true;
    st["restricted_until_date"] = seconds > 0 ? (int)(QDateTime::currentDateTime().toTime_t() + seconds) : 0;
    st["permissions"] = fill("chatPermissions", kPerms, perms, sub(m_chats.value(I(chatId)), "permissions"));
    M q = req("setChatMemberStatus"); q["chat_id"] = I(chatId); q["member_id"] = user(userId); q["status"] = st;
    send(q, "adm:restricted");
}

void Store::banMember(const QVariant &chatId, const QVariant &userId) {
    M q = req("banChatMember"); q["chat_id"] = I(chatId); q["member_id"] = user(userId); q["banned_until_date"] = 0; q["revoke_messages"] = false;
    send(q, "adm:banned");
}

void Store::unbanMember(const QVariant &chatId, const QVariant &userId) {
    M q = req("setChatMemberStatus"); q["chat_id"] = I(chatId); q["member_id"] = user(userId); q["status"] = req("chatMemberStatusLeft");
    send(q, "adm:unbanned");
}

void Store::loadBanned(const QVariant &chatId) {
    M type = sub(m_chats.value(I(chatId)), "type");
    if (T(type) != "chatTypeSupergroup") { Q_EMIT adminList("banned", QVariantList()); return; }
    M f = req("supergroupMembersFilterBanned"); f["query"] = "";
    M q = req("getSupergroupMembers"); q["supergroup_id"] = I(type.value("supergroup_id")); q["filter"] = f; q["offset"] = 0; q["limit"] = 200;
    send(q, "adm:bannedlist");
}

void Store::setPermissions(const QVariant &chatId, const QVariantMap &perms) {
    M q = req("setChatPermissions"); q["chat_id"] = I(chatId);
    q["permissions"] = fill("chatPermissions", kPerms, perms, sub(m_chats.value(I(chatId)), "permissions"));
    send(q, "adm:perms");
}

void Store::setSlowMode(const QVariant &chatId, int seconds) {
    M q = req("setChatSlowModeDelay"); q["chat_id"] = I(chatId); q["slow_mode_delay"] = seconds;
    send(q, "adm:slow");
}

void Store::saveChatInfo(const QVariant &chatId, const QString &title, const QString &description) {
    M c = m_chats.value(I(chatId));
    if (title.trimmed().isEmpty()) { Q_EMIT toast("The name can't be empty"); return; }
    if (title.trimmed() != c.value("title").toString()) { M q = req("setChatTitle"); q["chat_id"] = I(chatId); q["title"] = title.trimmed(); send(q, "adm:title"); }
    if (description != m_profileFull.value("description").toString()) { M q = req("setChatDescription"); q["chat_id"] = I(chatId); q["description"] = description; send(q, "adm:about"); }
}

void Store::setChatPhoto(const QVariant &chatId, const QString &path) {
    QString p = path.startsWith("file://") ? path.mid(7) : path;
    M file = req("inputFileLocal"); file["path"] = p;
    M photo = req("inputChatPhotoStatic"); photo["photo"] = file;
    M q = req("setChatPhoto"); q["chat_id"] = I(chatId); q["photo"] = photo;
    send(q, "adm:photo");
}

void Store::loadEventLog(const QVariant &chatId) {
    M q = req("getChatEventLog"); q["chat_id"] = I(chatId); q["query"] = ""; q["from_event_id"] = 0; q["limit"] = 100;
    q["user_ids"] = QVariantList();
    send(q, "adm:log");
}

// one admin-log entry as a line of text
QString Store::eventText(const M &e) {
    M a = sub(e, "action");
    QString t = T(a).startsWith("chatEvent") ? T(a).mid(9) : T(a);       // chatEventMemberJoined -> MemberJoined
    QString who = senderName(sub(e, "member_id"));
    QString target;
    if (a.contains("user_id")) target = userName(I(a.value("user_id")));
    else if (a.contains("member_id")) target = senderName(sub(a, "member_id"));
    if (t == "MessageEdited") return who + " edited a message";
    if (t == "MessageDeleted") return who + " deleted a message: " + preview(sub(a, "message"), false);
    if (t == "MessagePinned") return who + " pinned a message: " + preview(sub(a, "message"), false);
    if (t == "MessageUnpinned") return who + " unpinned a message";
    if (t == "MemberJoined" || t == "MemberJoinedByInviteLink" || t == "MemberJoinedByRequest") return who + " joined";
    if (t == "MemberLeft") return who + " left";
    if (t == "MemberInvited") return who + " added " + target;
    if (t == "MemberPromoted") {
        QString ns = T(sub(a, "new_status"));
        return ns == "chatMemberStatusAdministrator" ? who + " made " + target + " an admin" : who + " changed the rights of " + target;
    }
    if (t == "MemberRestricted") {
        QString ns = T(sub(a, "new_status"));
        if (ns == "chatMemberStatusBanned") return who + " removed " + target;
        if (ns == "chatMemberStatusRestricted") return who + " restricted " + target;
        return who + " lifted the restrictions of " + target;
    }
    if (t == "TitleChanged") return who + " renamed it to \"" + a.value("new_title").toString() + "\"";
    if (t == "DescriptionChanged") return who + " changed the description";
    if (t == "PhotoChanged") return who + " changed the photo";
    if (t == "PermissionsChanged") return who + " changed what members can do";
    if (t == "SlowModeDelayChanged") return who + " set slow mode to " + (a.value("new_slow_mode_delay").toInt() ? QString::number(a.value("new_slow_mode_delay").toInt()) + " s" : QString("off"));
    if (t == "UsernameChanged" || t == "ActiveUsernamesChanged") return who + " changed the public link";
    if (t.startsWith("InviteLink")) return who + " changed an invite link";
    if (t.startsWith("VideoChat")) return who + " changed a video chat";
    if (t == "PollStopped") return who + " stopped a poll";
    return who + " changed a setting";
}

void Store::onAdminAnswer(const QString &tag, const M &r) {
    bool error = T(r) == "error";
    if (error) {
        QString msg = r.value("message").toString();
        if (msg.contains("CHAT_ADMIN_REQUIRED") || msg.contains("RIGHT_FORBIDDEN")) msg = "You don't have the rights for this";
        else if (msg.contains("USER_NOT_MUTUAL_CONTACT")) msg = "This person can't be added back by you";
        else if (msg.contains("CHAT_NOT_MODIFIED")) return;
        Q_EMIT toast(msg);
        if (tag == "adm:bannedlist") Q_EMIT adminList("banned", QVariantList());
        if (tag == "adm:log") Q_EMIT adminList("log", QVariantList());
        if (tag.startsWith("adm:probe") || tag == "adm:bannedlist") qWarning() << "admintest: error on" << tag << msg.left(80);
        return;
    }
    QString w = tag.mid(4);
    if (w == "probelog") {
        QVariantList l = r.value("events").toList(); QStringList kinds;
        for (int i = 0; i < l.size(); i++) kinds << T(sub(l.at(i).toMap(), "action")).mid(15);
        qWarning() << "admintest: log entries" << l.size() << kinds.join(",").left(300) << "| first line ok:" << (!l.isEmpty() && !eventText(l.first().toMap()).isEmpty());
        M p = m_profile; qWarning() << "admintest: profile isAdmin" << p.value("isAdmin").toBool() << "owner" << p.value("isOwner").toBool()
            << "editInfo" << p.value("canEditInfo").toBool() << "restrict" << p.value("canRestrict").toBool() << "promote" << p.value("canPromote").toBool() << "log" << p.value("canLog").toBool();
        return;
    }
    if (w == "bannedlist") {
        QVariantList rows, l = r.value("members").toList();
        for (int i = 0; i < l.size(); i++) {
            M mm = l.at(i).toMap(), id = sub(mm, "member_id");
            if (T(id) != "messageSenderUser") continue;
            rows << userRow(I(id.value("user_id")), QString());
        }
        qWarning() << "admintest: removed users" << rows.size();
        Q_EMIT adminList("banned", rows);
        return;
    }
    if (w == "log") {
        QVariantList rows, l = r.value("events").toList();
        for (int i = 0; i < l.size(); i++) {
            M e = l.at(i).toMap(), row;
            row["text"] = eventText(e);
            uint d = e.value("date").toUInt();
            row["date"] = listDate(d) + ", " + clock(d);
            rows << row;
        }
        Q_EMIT adminList("log", rows);
        return;
    }
    static const char *const done[][2] = { { "promoted", "Admin rights saved" }, { "demoted", "No longer an admin" },
        { "restricted", "Restrictions saved" }, { "banned", "Removed" }, { "unbanned", "They can join again" }, { "perms", "Permissions saved" },
        { "slow", "Slow mode saved" }, { "title", "Name saved" }, { "about", "Description saved" }, { "photo", "Photo changed" }, { 0, 0 } };
    for (int i = 0; done[i][0]; i++) if (w == done[i][0]) Q_EMIT toast(done[i][1]);
    if (m_profileId) openProfile(m_profileId);               // members and roles again
}

// test aid: in the first supergroup where this account is an admin, load the admin log and the removed users.
// Read-only; logs only counts and the kinds of entries, no names or text.
void Store::adminProbe() {
    int groups = 0;
    qint64 found = 0;
    for (QHash<qint64, M>::const_iterator i = m_chats.constBegin(); i != m_chats.constEnd(); ++i) {
        M type = sub(i.value(), "type");
        if (T(type) != "chatTypeSupergroup") continue;
        groups++;
        QString st = T(sub(m_supergroups.value(I(type.value("supergroup_id"))), "status"));
        if (!found && (st == "chatMemberStatusCreator" || st == "chatMemberStatusAdministrator")) found = i.key();
    }
    qWarning() << "admintest: supergroups/channels" << groups << ", admin in one:" << (found != 0);
    if (!found) for (QHash<qint64, M>::const_iterator i = m_chats.constBegin(); i != m_chats.constEnd(); ++i)
        if (T(sub(i.value(), "type")) == "chatTypeSupergroup") { found = i.key(); break; }   // expect "admin required"
    if (!found) return;
    M q = req("getChatEventLog"); q["chat_id"] = found; q["query"] = ""; q["from_event_id"] = 0; q["limit"] = 20; q["user_ids"] = QVariantList();
    send(q, "adm:probelog");
    openProfile(found);
    loadBanned(found);
}
