// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_SESSION_DESKTOP_ENVIRONMENT_FACTORY_H_
#define REMOTING_HOST_WIN_SESSION_DESKTOP_ENVIRONMENT_FACTORY_H_

#include "remoting/host/desktop_environment_factory.h"

namespace remoting {

class ClientSession;

class SessionDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  SessionDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  virtual ~SessionDesktopEnvironmentFactory();

  virtual scoped_ptr<DesktopEnvironment> Create(ClientSession* client) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_SESSION_DESKTOP_ENVIRONMENT_FACTORY_H_
