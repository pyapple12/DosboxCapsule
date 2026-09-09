// SPDX-FileCopyrightText:  2026 CapsuleRetro project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_FRAME_H
#define DOSBOX_WEBSERVER_FRAME_H

#include "libs/http/http.h"

namespace Webserver {

// GET /api/v2/frame/stream：流式直推（HTTP chunked 长连接）。连接建立后每
// 捕获一帧即推送一个数据块：28 字节帧内头（帧号 u64 + 宽/高/行距 u32×3 +
// 捕获时刻 u64，全部 LE）+ 紧凑 BGRX 像素，客户端按头计算帧长切分流。
//（PL005：轮询版 GET /api/v2/frame 已随流式端点落地退役。）
void GetFrameStream(const httplib::Request& req, httplib::Response& res);

} // namespace Webserver

#endif // DOSBOX_WEBSERVER_FRAME_H
