// SPDX-FileCopyrightText:  2026 CapsuleRetro project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_INPUT_H
#define DOSBOX_WEBSERVER_INPUT_H

#include "libs/http/http.h"

namespace Webserver {

// POST /api/v2/input/event：注入合成输入事件（供统一窗口画布转发，PL002.25）。
// JSON body 三种 kind：
//   {"kind":"key","key":"return","mods":["alt"],"down":true}   键盘（走 mapper）
//   {"kind":"mouse","dx":5,"dy":-3}                            相对鼠标移动
//   {"kind":"click","button":"left","down":true}               鼠标按键
// 键盘经 Bridge 在模拟线程执行 MAPPER_CheckEvent（与真实事件泵同路）；
// 鼠标经 SDL_PushEvent 由主事件泵消费。
void PostInputEvent(const httplib::Request& req, httplib::Response& res);

} // namespace Webserver

#endif // DOSBOX_WEBSERVER_INPUT_H
