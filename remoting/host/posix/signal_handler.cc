// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jamiewalch): Add unit tests for this.

#include "remoting/host/posix/signal_handler.h"

#include <errno.h>
#include <signal.h>

#include <list>
#include <utility>

#include "base/compiler_specific.h"
#include "base/eintr_wrapper.h"
#include "base/message_loop.h"
#include "base/message_pump_libevent.h"
#include "base/threading/platform_thread.h"

namespace remoting {
namespace {

class SignalListener : public base::MessagePumpLibevent::Watcher {
 public:
  SignalListener();

  void AddSignalHandler(int signal, const SignalHandler& handler);

  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE {}

  // WatchFileDescriptor needs a controller through which the operation can be
  // canceled. We don't use it, but this is as good a place as any to store it.
  base::MessagePumpLibevent::FileDescriptorWatcher controller;

 private:
  typedef std::pair<int, SignalHandler> SignalAndHandler;
  typedef std::list<SignalAndHandler> SignalHandlers;
  SignalHandlers signal_handlers_;
};

SignalListener::SignalListener() {
}

void SignalListener::AddSignalHandler(int signal,
                                      const SignalHandler& handler) {
  signal_handlers_.push_back(SignalAndHandler(signal, handler));
}

void SignalListener::OnFileCanReadWithoutBlocking(int fd) {
  char buffer;
  int result = HANDLE_EINTR(read(fd, &buffer, sizeof(buffer)));
  if (result > 0) {
    for (SignalHandlers::const_iterator i = signal_handlers_.begin();
         i != signal_handlers_.end();
         ++i) {
      if (i->first == buffer) {
        i->second.Run(i->first);
      }
    }
  }
}

SignalListener* g_signal_listener = NULL;
int g_write_fd = 0;

void GlobalSignalHandler(int signal) {
  char byte = signal;
  int r ALLOW_UNUSED = write(g_write_fd, &byte, 1);
}

}  // namespace

// RegisterSignalHandler registers a signal handler that writes a byte to a
// pipe each time a signal is received. The read end of the pipe is registered
// with the current MessageLoop (which must be of type IO); whenever the pipe
// is readable, it invokes the specified callback.
//
// This arrangement is required because the set of system APIs that are safe to
// call from a signal handler is very limited (but does include write).
bool RegisterSignalHandler(int signal_number, const SignalHandler& handler) {
  CHECK(signal_number < 256);  // Don't want to worry about multi-byte writes.
  if (!g_signal_listener) {
    g_signal_listener = new SignalListener();
  }
  if (!g_write_fd) {
    int pipe_fd[2];
    int result = pipe(pipe_fd);
    if (result < 0) {
      LOG(ERROR) << "Could not create signal pipe: " << errno;
      return false;
    }
    MessageLoopForIO* message_loop = MessageLoopForIO::current();
    result = message_loop->WatchFileDescriptor(
        pipe_fd[0], true, MessageLoopForIO::WATCH_READ,
        &g_signal_listener->controller, g_signal_listener);
    if (!result) {
      LOG(ERROR) << "Failed to create signal detector task.";
      close(pipe_fd[0]);
      close(pipe_fd[1]);
      return false;
    }
    g_write_fd = pipe_fd[1];
  }
  if (signal(signal_number, GlobalSignalHandler) == SIG_ERR) {
    LOG(ERROR) << "signal() failed: " << errno;
    return false;
  }
  g_signal_listener->AddSignalHandler(signal_number, handler);
  return true;
}

}  // namespace remoting
