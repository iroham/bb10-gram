#!/usr/bin/env python3
"""QNX (BB10) port changes to the Telegram library source. Usage: td-patch.py <td source dir>. Safe to run twice."""
import sys, os
root = sys.argv[1]
def patch(path, old, new):
    p = os.path.join(root, path); s = open(p).read()
    if new in s: return
    assert old in s, (path, old[:50])
    open(p, "w").write(s.replace(old, new, 1))
# platform: BB10 is QNX Neutrino
patch("tdutils/td/utils/port/platform.h", "#elif defined(__linux__)\n  #define TD_LINUX 1", "#elif defined(__QNX__)\n  #define TD_QNX 1\n#elif defined(__linux__)\n  #define TD_LINUX 1")
# poll(2) and a socket pair, as on Solaris
patch("tdutils/td/utils/port/config.h", "#elif TD_SOLARIS\n  #define TD_POLL_POLL 1", "#elif TD_SOLARIS || TD_QNX\n  #define TD_POLL_POLL 1")
# the compiler is told "-std=c++17" (strict): QNX's headers then hide C99 functions libc++ needs (isblank...)
for std in ("14", "17", "20"):
    p = os.path.join(root, "CMake/TdSetUpCompiler.cmake"); t = open(p).read()
    open(p, "w").write(t.replace("-std=c++" + std, "-std=gnu++" + std))
# QNX keeps whole-second file times only
patch("tdutils/td/utils/port/Stat.cpp", '    T().warning("Platform lacks support of precise access/modification file times, comment this line to continue");\n',
      '    // (QNX: whole-second file times)\n')
# no alternate signal stack and no SA_RESTART on QNX
patch("tdutils/td/utils/port/signals.cpp", "#if TD_PORT_POSIX && !TD_DARWIN_TV_OS && !TD_DARWIN_WATCH_OS\n  auto page_size = getpagesize();",
      "#if TD_PORT_POSIX && !TD_DARWIN_TV_OS && !TD_DARWIN_WATCH_OS && !TD_QNX\n  auto page_size = getpagesize();")
patch("tdutils/td/utils/port/signals.cpp", "  act.sa_flags = SA_RESTART | SA_ONSTACK;", "#if TD_QNX\n  act.sa_flags = 0;\n#else\n  act.sa_flags = SA_RESTART | SA_ONSTACK;\n#endif")
# no fdopendir on QNX 8.0: open the directory by its path
patch("tdutils/td/utils/port/path.cpp", "  auto native_fd = fd.move_as_native_fd();\n  auto *subdir = fdopendir(native_fd.fd());",
      "#if TD_QNX\n  fd.close();\n  return walk_path_dir(path, func);\n#else\n  auto native_fd = fd.move_as_native_fd();\n  auto *subdir = fdopendir(native_fd.fd());")
patch("tdutils/td/utils/port/path.cpp", "  native_fd.release();\n  return walk_path_dir(path, subdir, func);\n}", "  native_fd.release();\n  return walk_path_dir(path, subdir, func);\n#endif\n}")
# poll(2) is level-triggered and TDLib's Poll backend treats it like epoll's edge mode: an idle connected socket is
# always writable, so poll() returned at once forever (gramd used ~45% of a core). Ask the kernel only for events the
# fd's owner does not know about yet (flags already reported, pending or not, are skipped) and leave out fds already
# known closed/errored. The owner runs on the same scheduler thread, so its flags are stable here.
patch("tdutils/td/utils/port/PollFlags.h", "  PollFlags read_flags_local() const;",
      "  PollFlags read_flags_local() const;\n  PollFlags read_flags_known() const {   // flags_ plus not yet flushed ones (QNX poll backend)\n    return PollFlags::from_raw(flags_.raw() | to_write_.load(std::memory_order_relaxed));\n  }")
patch("tdutils/td/utils/port/detail/PollableFd.h", "  PollFlags get_flags_local() const {\n    return flags_.read_flags_local();\n  }",
      "  PollFlags get_flags_local() const {\n    return flags_.read_flags_local();\n  }\n  PollFlags get_flags_known() const {\n    return flags_.read_flags_known();\n  }")
patch("tdutils/td/utils/port/detail/PollableFd.h", "inline PollFlags PollableFd::get_flags_unsafe() const {\n  return fd_info_->get_flags_local();\n}",
      "inline PollFlags PollableFd::get_flags_unsafe() const {\n  return fd_info_->get_flags_local();\n}\ninline PollFlags PollableFd::get_flags_known() const {\n  return fd_info_->get_flags_known();\n}")
patch("tdutils/td/utils/port/detail/PollableFd.h", "  PollFlags get_flags_unsafe() const;", "  PollFlags get_flags_unsafe() const;\n  PollFlags get_flags_known() const;")
patch("tdutils/td/utils/port/detail/Poll.h", "  vector<pollfd> pollfds_;\n  vector<PollableFd> fds_;",
      "  vector<pollfd> pollfds_;\n  vector<PollableFd> fds_;\n  vector<short> wanted_;   // events asked for at subscribe (QNX: pollfds_[i].events is recomputed per run)")
P = "tdutils/td/utils/port/detail/Poll.cpp"
patch(P, "  pollfd.revents = 0;\n  pollfds_.push_back(pollfd);\n  fds_.push_back(std::move(fd));",
      "  pollfd.revents = 0;\n  wanted_.push_back(pollfd.events);\n  pollfds_.push_back(pollfd);\n  fds_.push_back(std::move(fd));")
patch(P, "  for (auto it = pollfds_.begin(); it != pollfds_.end(); ++it) {\n    if (it->fd == fd.native_fd().fd()) {\n      pollfds_.erase(it);\n      fds_.erase(fds_.begin() + (it - pollfds_.begin()));\n      return;\n    }\n  }",
      "  for (size_t i = 0; i < fds_.size(); i++) {   // by the fd itself: pollfds_[i].fd may be -1 (skipped)\n    if (fds_[i].native_fd().fd() == fd.native_fd().fd()) {\n      pollfds_.erase(pollfds_.begin() + i);\n      fds_.erase(fds_.begin() + i);\n      wanted_.erase(wanted_.begin() + i);\n      return;\n    }\n  }")
patch(P, "void Poll::run(int timeout_ms) {\n  int err = poll(",
      "void Poll::run(int timeout_ms) {\n  for (size_t i = 0; i < pollfds_.size(); i++) {\n    PollFlags known = fds_[i].get_flags_known();\n    short ev = 0;\n    if ((wanted_[i] & POLLIN) && !known.can_read()) ev |= POLLIN;\n    if ((wanted_[i] & POLLOUT) && !known.can_write()) ev |= POLLOUT;\n    pollfds_[i].events = ev;\n    pollfds_[i].fd = known.can_close() || known.has_pending_error() ? -1 : fds_[i].native_fd().fd();\n  }\n  int err = poll(")
patch(P, "void Poll::clear() {\n  pollfds_.clear();\n}", "void Poll::clear() {\n  pollfds_.clear();\n  fds_.clear();\n  wanted_.clear();\n}")
