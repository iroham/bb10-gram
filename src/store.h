// Store: everything the screens show. It follows the Telegram library's updates (users, chats, positions in the
// chat lists, messages of the open chat, files) and offers the actions the screens need. Lists are Cascades
// DataModels whose rows are flat maps ready for QML.
#ifndef STORE_H
#define STORE_H
#include <bb/cascades/DataModel>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVariant>

class Store;
class TdClient;
class Calls;

// One chat list (main, archive): chats ordered as the library says (highest order first).
class ChatListModel : public bb::cascades::DataModel {
    Q_OBJECT
public:
    ChatListModel(Store *store, const QString &listType);
    virtual int childCount(const QVariantList &indexPath);
    virtual bool hasChildren(const QVariantList &indexPath);
    virtual QString itemType(const QVariantList &indexPath);
    virtual QVariant data(const QVariantList &indexPath);
    Q_INVOKABLE int size() const { return m_rows.size(); }
    void setPosition(qint64 chatId, qint64 order);     // order 0: not in this list
    void changed(qint64 chatId);
    void clear();
    QString listType() const { return m_listType; }
private:
    struct Row { qint64 order; qint64 id; };
    int find(qint64 chatId) const;
    Store *m_store;
    QString m_listType;
    QList<Row> m_rows;
};

// The messages of the open chat, newest first (the list is drawn bottom-up).
class MessageModel : public bb::cascades::DataModel {
    Q_OBJECT
public:
    explicit MessageModel(Store *store);
    virtual int childCount(const QVariantList &indexPath);
    virtual bool hasChildren(const QVariantList &indexPath);
    virtual QString itemType(const QVariantList &indexPath);
    virtual QVariant data(const QVariantList &indexPath);
    Q_INVOKABLE int size() const { return m_rows.size(); }
    void reset();
    void addNew(const QVariantMap &message);
    void addOlder(const QVariantList &messages);
    void replace(qint64 oldId, const QVariantMap &message);
    void remove(const QVariantList &ids);
    void touch(qint64 id);                       // redraw one row
    void touchAll();
    QVariantMap *find(qint64 id, int *index = 0);
    qint64 oldestId() const;
    qint64 newestId() const;
    int indexOf(qint64 id);
    QVariantList ids() const;
private:
    Store *m_store;
    QList<QVariantMap> m_rows;
};

class Store : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString authState READ authState NOTIFY authStateChanged)       // credentials, phone, code, password, ready, closed, other
    Q_PROPERTY(QVariantMap authInfo READ authInfo NOTIFY authStateChanged)     // details for the sign-in screens
    Q_PROPERTY(QString authError READ authError NOTIFY authErrorChanged)
    Q_PROPERTY(QString connection READ connection NOTIFY connectionChanged)    // "", "Connecting…", "Updating…", "Waiting for network…"
    Q_PROPERTY(QObject *chats READ chats CONSTANT)
    Q_PROPERTY(QObject *archive READ archive CONSTANT)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)       // [{key, title}], "main" = all chats
    Q_PROPERTY(QVariantMap me READ me NOTIFY meChanged)
    Q_PROPERTY(QVariantMap profile READ profile NOTIFY profileChanged)        // the chat/person shown on the info page
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY sessionsChanged)
    Q_PROPERTY(QVariantMap notifyScopes READ notifyScopes NOTIFY notifyScopesChanged) // {private, groups, channels}: true = notifications on
    Q_PROPERTY(bool hasNewer READ hasNewer NOTIFY hasNewerChanged)            // the open chat shows an older part (after a jump)
    Q_PROPERTY(QObject *messages READ messages CONSTANT)
    Q_PROPERTY(QVariantMap chat READ chat NOTIFY chatChanged)                  // the open chat: id, title, subtitle, photo, canSend...
    Q_PROPERTY(bool loadingHistory READ loadingHistory NOTIFY loadingHistoryChanged)
    Q_PROPERTY(int unread READ unread NOTIFY unreadChanged)
    Q_PROPERTY(QVariantList stickerSets READ stickerSets NOTIFY stickersChanged) // [{id, title}], id 0 = recent
    Q_PROPERTY(QVariantList stickers READ stickers NOTIFY stickersChanged)       // the shown set: [{image}]
    Q_PROPERTY(QVariantList gifs READ gifs NOTIFY stickersChanged)               // saved GIFs: [{image}]
    Q_PROPERTY(QVariant stickerSet READ stickerSet NOTIFY stickersChanged)       // id of the shown set
    Q_PROPERTY(QVariantList keyboard READ keyboard NOTIFY keyboardChanged)       // bot keyboard of the open chat: rows of [text]
    Q_PROPERTY(QVariantMap privacy READ privacy NOTIFY privacyChanged)           // key -> "Everybody" / "My contacts" / "Nobody" (+ exceptions)
    Q_PROPERTY(QVariantMap cover READ cover NOTIFY coverChanged)                 // minimized tile: {unread, line, rows:[{title, text, unread, muted}]}
    Q_PROPERTY(QVariantList stories READ stories NOTIFY storiesChanged)          // chats with active stories: [{chatId, title, photo, initials, color, unread, count}]
    Q_PROPERTY(QVariantMap story READ story NOTIFY storyChanged)                 // the story being watched (stories.cpp)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QObject *calls READ calls CONSTANT)                              // voice calls (calls.h)   // the app is in front
public:
    Store(TdClient *td, const QString &dataDir, QObject *parent = 0);
    QString authState() const { return m_authState; }
    QVariantMap authInfo() const { return m_authInfo; }
    QString authError() const { return m_authError; }
    QString connection() const { return m_connection; }
    QObject *chats() { return listFor("main"); }
    QObject *archive() { return listFor("archive"); }
    QVariantList folders() const { return m_folders; }
    QVariantMap me();
    QVariantMap profile() const { return m_profile; }
    QVariantList sessions() const { return m_sessions; }
    QVariantMap notifyScopes() const;
    bool hasNewer() const { return m_hasNewer; }
    QVariantList stickerSets() const { return m_stickerSets; }
    QVariantList stickers() const { return m_stickerRows; }
    QVariantList gifs() const { return m_gifRows; }
    QVariant stickerSet() const { return m_stickerSetId; }
    QVariantList keyboard() const { return m_keyboard; }
    QVariantMap privacy() const { return m_privacy; }
    QVariantMap cover() const { return m_cover; }
    QObject *messages() { return m_messages; }
    QVariantMap chat() const { return m_chat; }
    bool loadingHistory() const { return m_loadingHistory; }
    int unread() const { return m_unread; }
    bool active() const { return m_active; }
    void setActive(bool on);

    // sign-in
    Q_INVOKABLE void setCredentials(const QString &apiId, const QString &apiHash);
    Q_INVOKABLE void setPhone(const QString &phone);
    Q_INVOKABLE void checkCode(const QString &code);
    Q_INVOKABLE void checkPassword(const QString &password);
    Q_INVOKABLE void logOut();
    Q_INVOKABLE void resetCredentials();
    // chat lists
    Q_INVOKABLE void loadMoreChats(const QString &listKey);
    Q_INVOKABLE QObject *list(const QString &listKey) { return listFor(listKey); }
    // search: results arrive as searchDone(kind, rows); kind = chats | public | messages | inchat
    Q_INVOKABLE void searchAll(const QString &text);
    Q_INVOKABLE void searchInChat(const QString &text);
    // info page, account
    Q_INVOKABLE void openProfile(const QVariant &chatId);
    Q_INVOKABLE void openUserChat(const QVariant &userId);
    Q_INVOKABLE void loadContacts();
    Q_INVOKABLE void joinChat(const QVariant &chatId);
    Q_INVOKABLE void endSession(const QVariant &sessionId);
    Q_INVOKABLE void setScopeOn(const QString &scope, bool on);
    // the open chat
    Q_INVOKABLE void openChat(const QVariant &chatId);
    Q_INVOKABLE void closeChat();
    Q_INVOKABLE void loadOlder();
    Q_INVOKABLE void loadNewer();
    Q_INVOKABLE void jumpTo(const QVariant &messageId);
    Q_INVOKABLE void toLatest();
    Q_INVOKABLE void typing();
    Q_INVOKABLE void saveDraft(const QString &text);
    Q_INVOKABLE void react(const QVariant &messageId, const QString &emoji);
    Q_INVOKABLE void pinMessage(const QVariant &messageId, bool pin);
    Q_INVOKABLE void resendCode();
    Q_INVOKABLE void loadStickers();                 // sets + recent, when the panel opens
    Q_INVOKABLE void showStickerSet(const QVariant &setId);
    Q_INVOKABLE void sendSticker(int index);
    Q_INVOKABLE void loadGifs();
    Q_INVOKABLE void sendGif(int index);
    Q_INVOKABLE void pressKey(int row, int col);     // bot keyboard
    Q_INVOKABLE QString recordPath();                 // new file for a voice recording (in data/cache)
    Q_INVOKABLE void sendVoice(const QString &url, int seconds);
    Q_INVOKABLE void discardFile(const QString &url);
    Q_INVOKABLE void startSecretChat(const QVariant &userId);
    Q_INVOKABLE void setMuteFor(const QVariant &chatId, int seconds);   // 0 = unmute
    Q_INVOKABLE void removeMember(const QVariant &chatId, const QVariant &userId);
    // admin tools (admin.cpp); lists arrive as adminList(kind, rows), kind = banned | log
    // stories (stories.cpp)
    QVariantList stories();
    QVariantMap story() const { return m_story; }
    Q_INVOKABLE void openStories(const QVariant &chatId);
    Q_INVOKABLE bool nextStory();
    Q_INVOKABLE bool prevStory();
    Q_INVOKABLE void closeStories();
    Q_INVOKABLE void reactStory(const QString &emoji);
    Q_INVOKABLE void replyStory(const QString &text);
    Q_INVOKABLE QVariantMap memberInfo(const QVariant &userId);      // role, rights, permissions of a member of the profile's chat
    Q_INVOKABLE void promote(const QVariant &chatId, const QVariant &userId, const QVariantMap &rights);
    Q_INVOKABLE void demote(const QVariant &chatId, const QVariant &userId);
    Q_INVOKABLE void restrictMember(const QVariant &chatId, const QVariant &userId, const QVariantMap &perms, int seconds);
    Q_INVOKABLE void banMember(const QVariant &chatId, const QVariant &userId);
    Q_INVOKABLE void unbanMember(const QVariant &chatId, const QVariant &userId);
    Q_INVOKABLE void loadBanned(const QVariant &chatId);
    Q_INVOKABLE void setPermissions(const QVariant &chatId, const QVariantMap &perms);
    Q_INVOKABLE void setSlowMode(const QVariant &chatId, int seconds);
    Q_INVOKABLE void saveChatInfo(const QVariant &chatId, const QString &title, const QString &description);
    Q_INVOKABLE void setChatPhoto(const QVariant &chatId, const QString &path);
    Q_INVOKABLE void loadEventLog(const QVariant &chatId);
public Q_SLOTS:
    void adminProbe();
    void storyProbe();
    void heapProbe();                                             // test aid (debug heapTest): gramd logs its heap; loads more chats                                            // test aid (debug storyTest)                                            // test aid (debug adminTest): read-only checks, counts only
public:
    Q_INVOKABLE void loadMyInfo();
    Q_INVOKABLE void saveProfile(const QString &first, const QString &last, const QString &bio, const QString &username);
    Q_INVOKABLE void addMembers(const QVariant &chatId, const QVariantList &userIds);
    Q_INVOKABLE void copyInviteLink(const QVariant &chatId);
    Q_INVOKABLE void createChat(const QString &title, const QVariantList &userIds, bool channel);
    Q_INVOKABLE void loadPrivacy();
    Q_INVOKABLE void setPrivacy(const QString &key, int level);   // 0 everybody, 1 my contacts, 2 nobody
    Q_INVOKABLE void votePoll(const QVariant &messageId, int option);    // single choice: votes; multiple: toggles
    Q_INVOKABLE void sendVote(const QVariant &messageId);                // multiple choice: send the picked options
    Q_INVOKABLE void retractVote(const QVariant &messageId);
    Q_INVOKABLE void pressButton(const QVariant &messageId, int index);
    Q_INVOKABLE void openLink(const QString &url);
    Q_INVOKABLE void sendText(const QString &text, const QVariant &replyToId);
    Q_INVOKABLE void editText(const QVariant &messageId, const QString &text);
    Q_INVOKABLE void deleteMessage(const QVariant &messageId, bool forEveryone);
    Q_INVOKABLE void forwardMessage(const QVariant &messageId, const QVariant &toChatId);
    Q_INVOKABLE void sendFile(const QString &path, const QString &caption, bool asPhoto);
    Q_INVOKABLE void downloadFile(int fileId);
    Q_INVOKABLE QString plainText(const QVariant &messageId);
    Q_INVOKABLE void copyText(const QVariant &messageId);
    Q_INVOKABLE void copyTextRaw(const QString &text);
    Q_INVOKABLE void playAudio(const QVariant &messageId);
    Q_INVOKABLE QVariantMap message(const QVariant &messageId);
    // chat actions
    Q_INVOKABLE void setMuted(const QVariant &chatId, bool muted);
    Q_INVOKABLE void setPinned(const QVariant &chatId, bool pinned);
    Q_INVOKABLE void setArchived(const QVariant &chatId, bool archived);
    Q_INVOKABLE void markRead(const QVariant &chatId);
    Q_INVOKABLE void deleteChat(const QVariant &chatId);
    Q_INVOKABLE QVariantList findChats(const QString &text);
    Q_INVOKABLE QVariantMap chatInfo(const QVariant &chatId);

    // used by the models
    QVariantMap chatRow(qint64 chatId);
    QVariantMap messageRow(const QVariantMap &message, const QVariantMap *older);
    bool filePath(const QVariantMap &file, QString *path, bool wanted, qint64 forChat = 0, qint64 forMsg = 0);
    static QString kindOf(const QVariantMap &content);
    QString shownImage(const QVariantMap &file, const QString &format, qint64 msgId);
    void loadDemo(const QString &file);
    void openFromNotification(qint64 chatId);
    QObject *calls() const { return (QObject *)m_calls; }
    QVariantMap userCard(qint64 userId);          // name, initials, color, photo (big)
    friend class Calls;

Q_SIGNALS:
    void authStateChanged();
    void authErrorChanged();
    void connectionChanged();
    void chatChanged();
    void loadingHistoryChanged();
    void unreadChanged();
    void activeChanged();
    void toast(const QString &text);
    void foldersChanged();
    void meChanged();
    void profileChanged();
    void sessionsChanged();
    void notifyScopesChanged();
    void hasNewerChanged();
    void searchDone(const QString &kind, const QVariantList &rows);
    void scrollTo(int index);                    // show this row of the open chat
    void openChatRequested(const QVariant &chatId);
    void playFile(const QString &url);
    void stickersChanged();
    void newestAdded(bool outgoing);             // a new message at the bottom of the open chat
    void keyboardChanged();
    void privacyChanged();
    void coverChanged();
    void adminList(const QString &kind, const QVariantList &rows);
    void storiesChanged();
    void storyChanged();
public Q_SLOTS:
    void loadSessions();
private Q_SLOTS:
    void onUpdate(const QVariantMap &u);
    void onAnswer(const QString &tag, const QVariantMap &r);
    void typingOver();
    void sweepTyping();
    void rebuildStickers();
    void buildCover();

private:
    void send(const QVariantMap &request, const QString &tag = QString());
    void sendParameters();
    void setAuth(const QString &state, const QVariantMap &info = QVariantMap());
    void setPositions(qint64 chatId, const QVariantList &positions);
    ChatListModel *listFor(const QString &key);
    void buildProfile();
    void onAdminAnswer(const QString &tag, const QVariantMap &r);
    void loadStories();
    bool onStoryUpdate(const QString &t, const QVariantMap &u);
    void onStoryAnswer(const QString &tag, const QVariantMap &r);
    void showStory(int index);
    void buildStory();
    QHash<qint64, QVariantMap> m_activeStories;     // chat -> chatActiveStories (main list)
    QVariantMap m_story, m_storyData;
    qint64 m_storyChat;
    int m_storyId, m_storyIndex;
    QList<int> m_storyIds;
    QSet<int> m_storyFiles;
    int m_storyListCount;                          // TDLib's count of chats in the main story list (-1 unknown)
    QString m_storyLoads;                          // test aid: answers to loadActiveStories
    QString eventText(const QVariantMap &event);
    QVariantMap userRow(qint64 userId, const QString &extra);
    QVariantMap foundRow(const QVariantMap &message);
    void reloadLatest();
    void tellService();    void chatTouched(qint64 chatId);
    void refreshOpenChat();
    void viewOpenMessages();
    void notify(const QVariantMap &message);
    void setChatField(qint64 chatId, const char *key, const QVariant &value);
    void applyPosition(qint64 chatId, const QVariantMap &position);
    bool isMuted(const QVariantMap &chat) const;
    bool isGroup(const QVariantMap &chat) const;
    bool isChannel(const QVariantMap &chat) const;
    QString initials(const QString &title) const;
    QString userName(qint64 userId) const;
    QString senderName(const QVariantMap &sender) const;
    QString statusText(const QVariantMap &user) const;
    QString preview(const QVariantMap &message, bool withSender);
    QString contentSummary(const QVariantMap &content) const;
    static QString html(const QVariantMap &formattedText);
    static QString clock(uint t);
    static QString listDate(uint t);
    static QString dayName(uint t);
    static QString sizeText(qint64 bytes);

    TdClient *m_td;
    Calls *m_calls;
    QString m_dataDir;
    QString m_authState;
    QVariantMap m_authInfo;
    QString m_authError;
    QString m_connection;
    QHash<QString, ChatListModel *> m_lists;
    QVariantList m_folders;
    qint64 m_profileId;
    QVariantMap m_profile;
    QVariantMap m_profileFull;
    QVariantList m_profileMembers;
    QVariantList m_sessions;
    QHash<QString, QVariantMap> m_scopeSettings;
    QVariantMap m_pinned;
    QHash<qint64, QVariantMap> m_userFull;
    bool m_hasNewer;
    int m_searchSerial;
    qint64 m_lastTyping;
    qint64 m_pendingOpen;
    MessageModel *m_messages;
    QHash<qint64, QVariantMap> m_users;
    QHash<qint64, QVariantMap> m_chats;
    QHash<qint64, QVariantMap> m_basicGroups;
    QHash<qint64, QVariantMap> m_supergroups;
    QHash<int, QVariantMap> m_files;              // newest state of files we asked about
    QHash<int, QSet<qint64> > m_fileChats;        // file id -> chats whose row shows it
    QHash<int, QSet<qint64> > m_fileMsgs;         // file id -> messages of the open chat that show it
    QSet<int> m_asked;                            // downloads already requested
    QHash<QString, int> m_scopeMute;              // default mute per kind of chat
    QVariantMap m_demoHistory;
    bool m_demo;
    int m_histSerial;
    class QTimer *m_typingTimer;
    qint64 m_myId;
    qint64 m_openId;
    QVariantMap m_chat;
    QString m_typing;
    QHash<qint64, QPair<QString, qint64> > m_listTyping;  // chat -> ("Anna is typing…", expiry ms) for the list
    class QTimer *m_sweep;
    qint64 m_pendingJump;                                  // message to show once its chat is open (message links)
    qint64 m_unreadFrom;                                   // last read message when the chat was opened (unread line)
    QHash<qint64, QList<int> > m_pollPick;
    QVariantList m_gifRows, m_gifRaw;
    QVariantList m_stickerSets, m_stickerRows, m_stickerRaw, m_keyboard, m_keyboardRaw;
    qint64 m_stickerSetId;
    QSet<int> m_stickerFiles;                              // files the panel waits for
    class QTimer *m_stickerTimer;
    void loadKeyboard();
    QVariantMap m_privacy, m_privacyRules;
    QVariantMap m_cover;
    QHash<int, QVariantMap> m_secretChats;
    class QTimer *m_coverTimer;              // shown text; raw rules per key                 // multiple-choice polls: options picked, not sent yet
    bool m_loadingHistory;
    bool m_historyEnd;
    int m_unread;
    bool m_active;
};
#endif
