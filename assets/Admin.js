// Admin.js: the rights and permissions shown on SwitchListPage, in Telegram's order and words.
.pragma library

var groupRights = [
    ["can_change_info", "Change group info"], ["can_delete_messages", "Delete messages"], ["can_restrict_members", "Ban users"],
    ["can_invite_users", "Add members"], ["can_pin_messages", "Pin messages"], ["can_manage_video_chats", "Manage video chats"],
    ["can_promote_members", "Add new admins", "They can make others admins with their rights or fewer"],
    ["is_anonymous", "Remain anonymous", "Their messages show the group's name"]];
var channelRights = [
    ["can_change_info", "Change channel info"], ["can_post_messages", "Post messages"], ["can_edit_messages", "Edit messages of others"],
    ["can_delete_messages", "Delete messages of others"], ["can_post_stories", "Post stories"], ["can_edit_stories", "Edit stories of others"],
    ["can_delete_stories", "Delete stories of others"], ["can_invite_users", "Add subscribers"], ["can_manage_video_chats", "Manage live streams"],
    ["can_promote_members", "Add new admins", "They can make others admins with their rights or fewer"]];
var permissions = [
    ["can_send_basic_messages", "Send text messages"], ["can_send_photos", "Send photos"], ["can_send_videos", "Send videos"],
    ["can_send_video_notes", "Send video messages"], ["can_send_audios", "Send music"], ["can_send_voice_notes", "Send voice messages"],
    ["can_send_documents", "Send files"], ["can_send_other_messages", "Send stickers and GIFs"], ["can_send_polls", "Send polls"],
    ["can_add_link_previews", "Embed links"], ["can_invite_users", "Add members"], ["can_pin_messages", "Pin messages"],
    ["can_change_info", "Change group info"]];

// items for SwitchListPage: on from `values`; when `limit` is given (my own rights, not owner) only the rights I hold
function items(list, values, limit) {
    var out = [];
    for (var i = 0; i < list.length; i++) {
        var k = list[i][0];
        if (limit && k !== "is_anonymous" && limit[k] !== true) continue;
        out.push({ key: k, title: list[i][1], sub: list[i].length > 2 ? list[i][2] : "", on: values ? values[k] === true : false });
    }
    return out;
}

// a new admin starts with the usual rights
function defaults(channel) {
    return channel ? { can_change_info: true, can_post_messages: true, can_edit_messages: true, can_delete_messages: true, can_post_stories: true,
                       can_edit_stories: true, can_delete_stories: true, can_invite_users: true, can_manage_video_chats: true }
                   : { can_change_info: true, can_delete_messages: true, can_restrict_members: true, can_invite_users: true, can_pin_messages: true,
                       can_manage_video_chats: true };
}
