// SPDX-FileCopyrightText:  2026 CapsuleRetro project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_FRAME_H
#define DOSBOX_WEBSERVER_FRAME_H

#include "libs/http/http.h"

namespace Webserver {

// GET /api/v2/frame：返回最新完成的 DOS 帧。
// 响应头携带元数据（帧号/宽/高/行距），body 为紧凑 32bit BGRX 像素；
// 尚无帧可用时返回 503。
void GetFrame(const httplib::Request& req, httplib::Response& res);

} // namespace Webserver

#endif // DOSBOX_WEBSERVER_FRAME_H
