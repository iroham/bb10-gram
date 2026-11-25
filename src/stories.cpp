// Store, part: stories. The row of chats with active stories on top of the chat list (loadActiveStories +
// updateChatActiveStories), and the story being watched (getStory, openStory/closeStory, a reaction, a reply).
#include "store.h"
#include "tdclient.h"
#include <QDateTime>
#include <QStringList>
#include <QDebug>
#include <QTimer>

typedef QVariantMap M;
static inline qint64 I(const QVariant &v) { return v.toLongLong(); }
static inline QString T(const M &m) { return m.value("@type").toString(); }
static inline M sub(const M &m, const char *k) { return m.value(k).toMap(); }
static M req(const char *type) { M m; m["@type"] = QString::fromLatin1(type); return m; }

void Store::loadStories() {
    M r = req("loadActiveStories"); r["story_list"] = req("storyListMain");
    send(r, "stories");
}

// one update about stories; true when handled
bool Store::onStoryUpdate(const QString &t, const M &u) {
    if (t == "updateChatActiveStories") {
        M a = sub(u, "active_stories"); qint64 chat = I(a.value("chat_id"));
        if (a.value("stories").toList().isEmpty() || T(sub(a, "list")) != "storyListMain") m_activeStories.remove(chat);
        else m_activeStories[chat] = a;
        Q_EMIT storiesChanged();
        return true;
    }
    if (t == "updateStory") {
        M s = sub(u, "story");
        if (I(s.value("poster_chat_id")) == m_storyChat && s.value("id").toInt() == m_storyId) { m_storyData = s; buildStory(); }
        return true;
    }
    if (t == "updateStoryListChatCount") {
        if (T(sub(u, "story_list")) == "storyListMain") m_storyListCount = u.value("chat_count").toInt();
        return true;
    }
    if (t == "updateStoryDeleted" || t == "updateStorySendSucceeded" || t == "updateStorySendFailed"
        || t == "updateStoryStealthMode" || t == "updateStoryPostSucceeded" || t == "updateStoryPostFailed") return true;
    return false;
}

QVariantList Store::stories() {
    QMap<qint64, qint64> byOrder;                            // newest first: highest order
    for (QHash<qint64, M>::const_iterator i = m_activeStories.constBegin(); i != m_activeStories.constEnd(); ++i)
        byOrder.insertMulti(-I(i.value().value("order")), i.key());
    QVariantList rows;
    for (QMap<qint64, qint64>::const_iterator i = byOrder.constBegin(); i != byOrder.constEnd(); ++i) {
        M a = m_activeStories.value(i.value()), row;
        QVariantList l = a.value("stories").toList();
        int maxRead = a.value("max_read_story_id").toInt(), last = l.isEmpty() ? 0 : l.last().toMap().value("story_id").toInt();
        M c = chatRow(i.value());
        row["chatId"] = i.value();
        row["title"] = i.value() == m_myId ? QString("My story") : c.value("title").toString().section(' ', 0, 0);
        row["photo"] = c.value("photo"); row["initials"] = c.value("initials"); row["color"] = c.value("color");
        row["unread"] = last > maxRead;
        row["count"] = l.size();
        rows << row;
    }
    // unseen first (Telegram's order), keeping the order inside each half
    QVariantList unseen, seen;
    for (int i = 0; i < rows.size(); i++) (rows.at(i).toMap().value("unread").toBool() ? unseen : seen) << rows.at(i);
    return unseen + seen;
}

void Store::openStories(const QVariant &chatId) {
    qint64 chat = I(chatId);
    M a = m_activeStories.value(chat);
    QVariantList l = a.value("stories").toList();
    if (l.isEmpty()) { Q_EMIT storyChanged(); return; }
    m_storyChat = chat; m_storyIds.clear();
    int maxRead = a.value("max_read_story_id").toInt(), start = 0;
    for (int i = 0; i < l.size(); i++) m_storyIds << l.at(i).toMap().value("story_id").toInt();
    for (int i = 0; i < m_storyIds.size(); i++) if (m_storyIds.at(i) > maxRead) { start = i; break; }
    showStory(start);
}

void Store::showStory(int index) {
    if (m_storyId) { M c = req("closeStory"); c["story_poster_chat_id"] = m_storyChat; c["story_id"] = m_storyId; send(c, "q"); }
    m_storyIndex = index; m_storyId = m_storyIds.value(index); m_storyData = M(); m_storyFiles.clear();
    buildStory();
    M g = req("getStory"); g["story_poster_chat_id"] = m_storyChat; g["story_id"] = m_storyId; g["only_local"] = false;
    send(g, "story:" + QString::number(m_storyChat) + ":" + QString::number(m_storyId));
    M o = req("openStory"); o["story_poster_chat_id"] = m_storyChat; o["story_id"] = m_storyId;   // marks it seen
    send(o, "q");
}

// next story; at the end, the next chat with stories. false = nothing more (close the viewer)
bool Store::nextStory() {
    if (m_storyIndex + 1 < m_storyIds.size()) { showStory(m_storyIndex + 1); return true; }
    QVariantList rows = stories();
    for (int i = 0; i < rows.size(); i++) {
        if (I(rows.at(i).toMap().value("chatId")) != m_storyChat) continue;
        for (int j = i + 1; j < rows.size(); j++) {
            if (!rows.at(j).toMap().value("unread").toBool()) continue;       // only on to stories not seen yet
            openStories(rows.at(j).toMap().value("chatId"));
            return true;
        }
    }
    return false;
}

bool Store::prevStory() {
    if (m_storyIndex > 0) { showStory(m_storyIndex - 1); return true; }
    return false;
}

void Store::closeStories() {
    if (m_storyId) { M c = req("closeStory"); c["story_poster_chat_id"] = m_storyChat; c["story_id"] = m_storyId; send(c, "q"); }
    m_storyId = 0; m_storyChat = 0; m_storyIds.clear(); m_storyData = M(); m_storyFiles.clear();
}

void Store::reactStory(const QString &emoji) {
    if (!m_storyId) return;
    M rt = req("reactionTypeEmoji"); rt["emoji"] = emoji;
    M r = req("setStoryReaction"); r["story_poster_chat_id"] = m_storyChat; r["story_id"] = m_storyId; r["reaction_type"] = rt; r["update_recent_reactions"] = true;
    send(r, "storyreact");
}

void Store::replyStory(const QString &text) {
    if (!m_storyId || text.trimmed().isEmpty()) return;
    M ft = req("formattedText"); ft["text"] = text.trimmed(); ft["entities"] = QVariantList();
    M content = req("inputMessageText"); content["text"] = ft;
    M to = req("inputMessageReplyToStory"); to["story_poster_chat_id"] = m_storyChat; to["story_id"] = m_storyId;
    M r = req("sendMessage"); r["chat_id"] = m_storyChat; r["reply_to"] = to; r["input_message_content"] = content;
    send(r, "storyreply");
}

void Store::onStoryAnswer(const QString &tag, const M &r) {
    bool error = T(r) == "error";
    if (tag == "stories") {                                  // keep loading until TDLib says all are here (404)
        m_storyLoads += error ? QString("E") + r.value("code").toString() + " " : QString("ok ");
        if (!error) loadStories();
        return;
    }
    if (tag == "storyprobe") {
        if (error) { qWarning() << "storytest: getStory error" << r.value("message").toString(); return; }
        M content = sub(r, "content");
        qWarning() << "storytest: story kind" << T(content) << ", can reply" << r.value("can_be_replied").toBool()
                   << ", caption chars" << sub(r, "caption").value("text").toString().size();
        return;
    }
    if (tag == "storyreact") { if (error) Q_EMIT toast("Couldn't react: " + r.value("message").toString()); return; }
    if (tag == "storyreply") { Q_EMIT toast(error ? "Couldn't send: " + r.value("message").toString() : QString("Reply sent")); return; }
    QStringList p = tag.split(':');                          // story:<chat>:<id>
    if (p.size() != 3 || I(p.at(1)) != m_storyChat || p.at(2).toInt() != m_storyId) return;
    if (error) { m_storyData = M(); m_storyData["error"] = r.value("message"); buildStory(); return; }
    m_storyData = r;
    buildStory();
}

// the story on screen: {chatId, id, index, count, title, avatar, time, kind photo|video|other, image, video,
// duration, caption, canReply, reaction, loading}
void Store::buildStory() {
    M s, c = chatRow(m_storyChat);
    s["chatId"] = m_storyChat; s["id"] = m_storyId; s["index"] = m_storyIndex; s["count"] = m_storyIds.size();
    s["title"] = m_storyChat == m_myId ? QString("My story") : c.value("title").toString();
    s["photo"] = c.value("photo"); s["initials"] = c.value("initials"); s["color"] = c.value("color");
    s["loading"] = m_storyData.isEmpty();
    s["error"] = m_storyData.value("error").toString();
    if (!m_storyData.isEmpty() && !m_storyData.contains("error")) {
        uint d = m_storyData.value("date").toUInt();
        qint64 ago = QDateTime::currentDateTime().toTime_t() - (qint64)d;
        s["time"] = ago < 60 ? QString("just now") : ago < 3600 ? QString::number(ago / 60) + " min ago" : QString::number(ago / 3600) + " h ago";
        s["caption"] = sub(m_storyData, "caption").value("text").toString();
        s["canReply"] = m_storyData.value("can_be_replied").toBool();
        M rt = sub(m_storyData, "chosen_reaction_type");
        s["reaction"] = rt.value("emoji").toString();
        M content = sub(m_storyData, "content"); QString ct = T(content);
        QString image, video;
        if (ct == "storyContentPhoto") {
            s["kind"] = "photo";
            QVariantList sizes = sub(content, "photo").value("sizes").toList(); M best;
            for (int i = 0; i < sizes.size(); i++) { M z = sizes.at(i).toMap(); if (best.isEmpty() || z.value("width").toInt() <= 1280) best = z; }
            m_storyFiles.insert(sub(best, "photo").value("id").toInt());
            filePath(sub(best, "photo"), &image, true);
            s["duration"] = 6;
        } else if (ct == "storyContentVideo") {
            s["kind"] = "video";
            M v = sub(content, "video");
            M th = sub(v, "thumbnail");
            filePath(sub(th, "file"), &image, true);
            m_storyFiles.insert(sub(v, "video").value("id").toInt()); m_storyFiles.insert(sub(th, "file").value("id").toInt());
            filePath(sub(v, "video"), &video, true);
            s["duration"] = qMax(3, (int)(v.value("duration").toDouble() + 0.5));
        } else {
            s["kind"] = "other"; s["duration"] = 6;
        }
        s["image"] = image; s["video"] = video;
    }
    m_story = s;
    Q_EMIT storyChanged();
}

// test aid (debug storyTest): how many chats have stories, and fetch the first one's first story without opening
// it (getStory doesn't mark it seen or tell the poster). Logs counts and kinds only.
void Store::storyProbe() {
    QVariantList rows = stories();
    int unread = 0;
    for (int i = 0; i < rows.size(); i++) if (rows.at(i).toMap().value("unread").toBool()) unread++;
    qWarning() << "storytest: chats with stories" << rows.size() << ", not seen" << unread << ", TDLib main list count" << m_storyListCount
               << ", load answers" << m_storyLoads;
    if (rows.isEmpty()) return;
    qint64 chat = I(rows.first().toMap().value("chatId"));
    QVariantList l = m_activeStories.value(chat).value("stories").toList();
    M g = req("getStory"); g["story_poster_chat_id"] = chat; g["story_id"] = l.first().toMap().value("story_id"); g["only_local"] = false;
    send(g, "storyprobe");
}

// test aid (debug heapTest): every call asks gramd to log its heap and loads more of the chat list (read-only)
void Store::heapProbe() {
    static int n = 0;
    n++;
    if (m_td->remote()) { M c; c["@ui"] = "heap"; c["when"] = QString("probe %1, chats %2").arg(n).arg(m_chats.size()); m_td->control(c); }
    if (n < 8) { loadMoreChats("main"); loadMoreChats("archive"); QTimer::singleShot(15000, this, SLOT(heapProbe())); }
}
